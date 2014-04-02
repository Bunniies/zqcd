/*
 * NFlavorQCDUpdater.cpp
 *
 *  Created on: May 2, 2012
 *      Author: spiem_01
 */

#include "NFlavorQCDUpdater.h"
#include "NFlavorQCDAction.h"
#include "AlgebraUtils.h"
#include "MultishiftSolver.h"
#include "RationalApproximation.h"
#include "BiConjugateGradient.h"
#include "DiracEigenSolver.h"
#include "GaugeAction.h"
#include "Integrate.h"
#include "ToString.h"
#include "GlobalOutput.h"
#include <iomanip>

//#define DEBUGFORCE
//#define REVERSIBILITY_CHECK

#ifdef DEBUGFORCE
#include "TestForce.h"
#endif

namespace Update {

NFlavorQCDUpdater::NFlavorQCDUpdater() : LatticeSweep(), nFlavorQCDAction(0), gaugeAction(0), fermionAction(0), squareDiracOperator(0), diracOperator(0) { }

NFlavorQCDUpdater::NFlavorQCDUpdater(const NFlavorQCDUpdater& toCopy) : LatticeSweep(toCopy), nFlavorQCDAction(0), gaugeAction(0), fermionAction(0), squareDiracOperator(0), diracOperator(0) { }

NFlavorQCDUpdater::~NFlavorQCDUpdater() {
	if (nFlavorQCDAction != 0) delete nFlavorQCDAction;
}

void NFlavorQCDUpdater::initializeApproximations(environment_t& environment) {
	//First take the rational function approximation for the heatbath step
	if (rationalApproximationsHeatBath.empty()) {
		int numberPseudofermions = environment.configurations.get< unsigned int >("number_pseudofermions");
		for (int i = 1; i <= numberPseudofermions; ++i) {
			std::vector<real_t> rat = environment.configurations.get< std::vector<real_t> >(std::string("heatbath_rational_fraction_")+toString(i));
			RationalApproximation rational;
			rational.setAlphas(std::vector<real_t>(rat.begin(), rat.begin() + rat.size()/2));
			rational.setBetas(std::vector<real_t>(rat.begin() + rat.size()/2, rat.end()));
			rational.setPrecision(environment.configurations.get<double>("metropolis_inverter_precision"));
			rational.setMaximumRecursion(environment.configurations.get<unsigned int>("metropolis_inverter_max_steps"));
			rationalApproximationsHeatBath.push_back(rational);
		}
		pseudofermions.resize(numberPseudofermions);
	}

	//Then take the rational function approximation for the metropolis step
	if (rationalApproximationsMetropolis.empty()) {
		int numberPseudofermions = environment.configurations.get< unsigned int >("number_pseudofermions");
		for (int i = 1; i <= numberPseudofermions; ++i) {
			std::vector<real_t> rat = environment.configurations.get< std::vector<real_t> >(std::string("metropolis_rational_fraction_")+toString(i));
			RationalApproximation rational;
			rational.setAlphas(std::vector<real_t>(rat.begin(), rat.begin() + rat.size()/2));
			rational.setBetas(std::vector<real_t>(rat.begin() + rat.size()/2, rat.end()));
			rational.setPrecision(environment.configurations.get<double>("metropolis_inverter_precision"));
			rational.setMaximumRecursion(environment.configurations.get<unsigned int>("metropolis_inverter_max_steps"));
			rationalApproximationsMetropolis.push_back(rational);
		}
	}

	//Then take the rational function approximation for the force step
	if (rationalApproximationsForce.empty()) {
		int numberPseudofermions = environment.configurations.get< unsigned int >("number_pseudofermions");
		for (int i = 1; i <= numberPseudofermions; ++i) {
			std::vector<real_t> rat = environment.configurations.get< std::vector<real_t> >(std::string("force_rational_fraction_")+toString(i));
			RationalApproximation rational;
			rational.setAlphas(std::vector<real_t>(rat.begin(), rat.begin() + rat.size()/2));
			rational.setBetas(std::vector<real_t>(rat.begin() + rat.size()/2, rat.end()));
			rational.setPrecision(environment.configurations.get<double>("force_inverter_precision"));
			rational.setMaximumRecursion(environment.configurations.get<unsigned int>("force_inverter_max_steps"));
			rationalApproximationsForce.push_back(rational);
		}
	}
}

void NFlavorQCDUpdater::checkTheory(const environment_t& environment) const {
	double testerForce = 1., testerMetropolis = 1., testerHeatBath = 1.;
	double epsilon = 0.0001;
	int numberPseudofermions = environment.configurations.get< unsigned int >("number_pseudofermions");
	for (int i = 0; i < numberPseudofermions; ++i) {
		testerForce *= rationalApproximationsForce[i].evaluate(2.).real();
		testerMetropolis *= rationalApproximationsMetropolis[i].evaluate(2.).real();
		testerHeatBath *= rationalApproximationsHeatBath[i].evaluate(2.).real();
	}
	double numberFermions = -2*log(testerMetropolis)/log(2.);//We are using the square of the dirac operator, hence we have a factor 2
	if (isOutputProcess()) std::cout << "NFlavorQCDUpdater::The theory has " <<  numberFermions << " nf." << std::endl;
#ifdef ADJOINT
	if (isOutputProcess() && fabs(numberFermions - 0.5) < epsilon) std::cout << "NFlavorQCDUpdater::The theory seems SUSY" << std::endl;
#endif
#ifndef ADJOINT
	if (isOutputProcess() && fabs(numberFermions - 1) < epsilon) std::cout << "NFlavorQCDUpdater::The theory seems 1 FlavorQCD" << std::endl;
	if (isOutputProcess() && fabs(numberFermions - 2) < epsilon) std::cout << "NFlavorQCDUpdater::The theory seems 2 FlavorQCD" << std::endl;
#endif
	if (isOutputProcess() && fabs(testerMetropolis/testerForce - 1.) > epsilon) {
		std::cout << "NFlavorQCDUpdater::Warning, large mismatch between force and metropolis approximations: " << fabs(testerMetropolis/testerForce - 1.) << std::endl;
	}
	if (isOutputProcess() && fabs(testerMetropolis*testerHeatBath*testerHeatBath - 1.) > epsilon) {
		std::cout << "NFlavorQCDUpdater::Warning, large mismatch between heatbath and metropolis approximations: " << fabs(testerMetropolis*testerHeatBath*testerHeatBath - 1.) << " " << testerMetropolis << " " << testerHeatBath << std::endl;
	}
}

void NFlavorQCDUpdater::execute(environment_t& environment) {	
	//First we initialize the approximations
	this->initializeApproximations(environment);

	//We check the theory that is simulated
	if (environment.iteration == 0 && environment.sweep == 0) {
		this->checkTheory(environment);
	}

	//Initialize the momenta
	this->randomMomenta(momenta);
	//Copy the environment
	environmentNew = environment;

	//Take the Dirac Operator
	if (diracOperator == 0)  diracOperator = DiracOperator::getInstance(environment.configurations.get<std::string>("dirac_operator"), 1, environment.configurations);
	diracOperator->setLattice(environment.getFermionLattice());

	//Take the Square Dirac Operator
	if (squareDiracOperator == 0) squareDiracOperator = DiracOperator::getInstance(environment.configurations.get<std::string>("dirac_operator"), 2, environment.configurations);
	squareDiracOperator->setLattice(environment.getFermionLattice());

	//Take the gauge action
	if (gaugeAction == 0) gaugeAction = GaugeAction::getInstance(environment.configurations.get<std::string>("name_action"),environment.configurations.get<double>("beta"));

	//Initialize the pseudofermion fields
	std::vector<extended_dirac_vector_t>::iterator i;
	std::vector<RationalApproximation>::iterator j = rationalApproximationsHeatBath.begin();
	long_real_t oldPseudoFermionEnergy = 0.;
	for (i = pseudofermions.begin(); i != pseudofermions.end(); ++i) {
		this->generateGaussianDiracVector(tmp_pseudofermion);
		oldPseudoFermionEnergy += AlgebraUtils::squaredNorm(tmp_pseudofermion);
		//Heat bath by multiply the tmp_pseudofermion to the polynomial of the dirac operator
		j->evaluate(squareDiracOperator, *i, tmp_pseudofermion);//TODO
		++j;
	}

	try {
		std::string flag = environment.configurations.get<std::string>("check_rational_approximations");

		if (environment.sweep == 0 && environment.iteration == 0 && environment.measurement && flag == "true") {
			//Now we test the correctness of rational/heatbath
			long_real_t test = 0.;
			//Now we use the better approximation for the metropolis step
			std::vector<RationalApproximation>::iterator rational = rationalApproximationsMetropolis.begin();
			for (i = pseudofermions.begin(); i != pseudofermions.end(); ++i) {
				//Now we evaluate it with the rational approximation of the inverse
				rational->evaluate(squareDiracOperator,tmp_pseudofermion,*i);
				test += real(AlgebraUtils::dot(*i,tmp_pseudofermion));
				++rational;
			}
			if (isOutputProcess()) std::cout << "NFlavoQCDUpdater::Consistency check for the metropolis: " << test - oldPseudoFermionEnergy << std::endl;
			//Now we use the approximation for the force step
			rational = rationalApproximationsForce.begin();
			test = 0.;
			for (i = pseudofermions.begin(); i != pseudofermions.end(); ++i) {
				//Now we evaluate it with the rational approximation of the inverse
				rational->evaluate(squareDiracOperator,tmp_pseudofermion,*i);
				test += real(AlgebraUtils::dot(*i,tmp_pseudofermion));
				++rational;
			}
			if (isOutputProcess()) std::cout << "NFlavoQCDUpdater::Consistency check for the force: " << test - oldPseudoFermionEnergy << std::endl;
		}

	} catch (NotFoundOption& ex) {
		std::cout << "NFlavorQCDUpdater::No consistency check of metropolis/force approximations!" << std::endl;
	}

	//Get the initial energy of momenta
	long_real_t oldMomentaEnergy = this->momentaEnergy(momenta);
	//Get the initial energy of the lattice
	long_real_t oldLatticeEnergy = gaugeAction->energy(environment);

	//Set the dirac operator to the new environment
	squareDiracOperator->setLattice(environmentNew.getFermionLattice());
	diracOperator->setLattice(environmentNew.getFermionLattice());

	//Take the fermion action
	if (fermionAction == 0) fermionAction = new NFlavorFermionAction(squareDiracOperator, diracOperator, rationalApproximationsForce);
	//TODO dirac operator correctly set?
	for (i = pseudofermions.begin(); i != pseudofermions.end(); ++i) {
		//Add the pseudofermions
		fermionAction->addPseudoFermion(&(*i));
	}
	//Set the precision of the inverter
	fermionAction->setForcePrecision(environment.configurations.get<double>("force_inverter_precision"));
	fermionAction->setForceMaxIterations(environment.configurations.get<unsigned int>("force_inverter_max_steps"));

	//Take the global action
	if (nFlavorQCDAction == 0) nFlavorQCDAction = new NFlavorAction(gaugeAction, fermionAction);

	//The t-length of a single integration step
	real_t t_length = environment.configurations.get<double>("hmc_t_length");
	std::vector<Force*> forces;
	//The numbers of integration steps
	std::vector<unsigned int> numbers_steps = environment.configurations.get< std::vector<unsigned int> >("number_hmc_steps");
	if (numbers_steps.size() == 1) {
		forces.push_back(nFlavorQCDAction);
	} else if (numbers_steps.size() == 2) {
		forces.push_back(fermionAction);
		forces.push_back(gaugeAction);
	}
	else {
		if (isOutputProcess()) std::cout << "NFlavorHMCUpdater::Warning, NFlavor does not support more than two time integrations!" << std::endl;
		numbers_steps.resize(1);
		forces.push_back(nFlavorQCDAction);
	}

#ifdef DEBUGFORCE
	//Test of the force
	TestForce testForce;
	extended_gauge_lattice_t tmp;
	//Internal calculations needed
	nFlavorQCDAction->updateForce(tmp, environment);
	testForce.genericTestForce(environment, nFlavorQCDAction, tmp[5][2], 5, 2);
#endif

	Integrate* integrate = Integrate::getInstance(environment.configurations.get<std::string>("name_integrator"));

	//Integrate numerically the equation of motion
	integrate->integrate(environmentNew, momenta, forces, numbers_steps, t_length);
#ifdef REVERSIBILITY_CHECK
	integrate->integrate(environmentNew, momenta, forces, numbers_steps, -t_length);
#endif

	//Get the final energy of momenta
	long_real_t newMomentaEnergy = this->momentaEnergy(momenta);
	//Get the final energy of the lattice
	long_real_t newLatticeEnergy = gaugeAction->energy(environmentNew);
	//Get the final energy of the pseudofermions
	long_real_t newPseudoFermionEnergy = 0.;
	diracOperator->setLattice(environmentNew.getFermionLattice());//TODO TODO TODO
	squareDiracOperator->setLattice(environmentNew.getFermionLattice());
	//Now we use the better approximation for the metropolis step
	std::vector<RationalApproximation>::iterator rational = rationalApproximationsMetropolis.begin();
	for (i = pseudofermions.begin(); i != pseudofermions.end(); ++i) {
		//Now we evaluate it with the rational approximation of the inverse
		rational->evaluate(squareDiracOperator,tmp_pseudofermion,*i);
		newPseudoFermionEnergy += real(AlgebraUtils::dot(*i,tmp_pseudofermion));
		++rational;
	}

	//Global Metropolis Step
	bool metropolis = this->metropolis(oldMomentaEnergy + oldLatticeEnergy + oldPseudoFermionEnergy, newMomentaEnergy + newLatticeEnergy + newPseudoFermionEnergy);

	if (metropolis) {
		environment = environmentNew;
		if (environment.measurement && isOutputProcess()) {
			GlobalOutput* output = GlobalOutput::getInstance();

			output->push("hmc_history");

			output->write("hmc_history", - (oldMomentaEnergy + oldLatticeEnergy + oldPseudoFermionEnergy) + (newMomentaEnergy + newLatticeEnergy + newPseudoFermionEnergy));
			output->write("hmc_history", 1);

			output->pop("hmc_history");
		}
	}
	else {
		if (environment.measurement && isOutputProcess()) {
			GlobalOutput* output = GlobalOutput::getInstance();

			output->push("hmc_history");

			output->write("hmc_history", - (oldMomentaEnergy + oldLatticeEnergy + oldPseudoFermionEnergy) + (newMomentaEnergy + newLatticeEnergy + newPseudoFermionEnergy));
			output->write("hmc_history", 0);

			output->pop("hmc_history");
		}
	}

	delete integrate;

	/*
	DiracOperator* diracOperator2 = new SquareDiracWilsonOperator(&environment.gaugeLinkConfiguration, environment.configurations.get<double>("kappa"));

	dirac_vector_t tmp1, tmp2, tmp3, tmp_pseudofermion;

	std::vector<complex> pol = environment.configurations.get< std::vector<complex> >("heatbath_polynomial");
	Polynomial polynomial;
	polynomial.setScaling(pol.front());
	pol.erase(pol.begin());
	polynomial.setRoots(pol);

	std::cout << "Rational 1/sqrt(1): " << std::setprecision(15) << rational.evaluate(complex(1,0.)) << std::endl;

	//std::cout << "Vediamo la nostra morte 0: " << std::endl << pseudofermions.front()[5][3] << std::endl;

	//polynomial.evaluate(diracOperator, tmp1, pseudofermions.front());

	this->generateGaussianDiracVector(tmp_pseudofermion);

	//polynomial.evaluate(diracOperator, tmp1, tmp_pseudofermion);

	BiConjugateGradient* biConjugateGradient = BiConjugateGradient::getInstance();
	//
	biConjugateGradient->solve(diracOperator, tmp_pseudofermion, tmp3);

	diracOperator = new SquareDiracWilsonOperator();
	diracOperator->setLattice(&environment.gaugeLinkConfiguration);
	diracOperator->setKappa(environment.configurations.get<double>("kappa"));

	//std::cout << "Vediamo la nostra morte 01: " << std::endl << tmp_pseudofermion[5][3] << std::endl;

	rational.evaluate(diracOperator, tmp1, tmp_pseudofermion);
	//rational.evaluate(diracOperator, tmp2, tmp1);
	polynomial.evaluate(diracOperator, tmp2, tmp1);
	//biConjugateGradient->setPrecision(0.00000000000001);
	biConjugateGradient->solve(diracOperator, tmp_pseudofermion, tmp3);
	//diracOperator->multiply(tmp3,tmp2);

	std::cout << "Vediamo la nostra morte 1: " << std::endl << tmp1[5][3] << std::endl;
	std::cout << "Vediamo la nostra morte 2: " << std::endl << tmp2[5][3] << std::endl;
	std::cout << "Vediamo la nostra morte 3: " << std::endl << tmp_pseudofermion[5][3] << std::endl;
	*/
}

} /* namespace Update */
