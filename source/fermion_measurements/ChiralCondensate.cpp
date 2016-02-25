/*
 * ChiralCondensate.cpp
 *
 *  Created on: Jul 23, 2012
 *      Author: spiem_01
 */

#include "ChiralCondensate.h"
#include "algebra_utils/AlgebraUtils.h"
#include "inverters/BiConjugateGradient.h"
#include "io/GlobalOutput.h"

namespace Update {

ChiralCondensate::ChiralCondensate() : LatticeSweep(), StochasticEstimator(), diracOperator(0), gmresr(0) { }

ChiralCondensate::ChiralCondensate(const ChiralCondensate& toCopy) : LatticeSweep(toCopy), StochasticEstimator(toCopy), diracOperator(0), gmresr(0) { }

ChiralCondensate::~ChiralCondensate() {
	if (diracOperator) delete diracOperator;
	if (gmresr) delete gmresr;
}

void ChiralCondensate::execute(environment_t& environment) {
	unsigned int max_step = environment.configurations.get<unsigned int>("ChiralCondensate::number_stochastic_estimators");
	
	if (diracOperator == 0) {
		diracOperator = DiracOperator::getInstance(environment.configurations.get<std::string>("dirac_operator"), 1, environment.configurations);
	}
	diracOperator->setLattice(environment.getFermionLattice());
	diracOperator->setGamma5(false);
	
	long_real_t volume = environment.gaugeLinkConfiguration.getLayout().globalVolume;

	if (gmresr == 0) {
		gmresr = new GMRESR();
		gmresr->setMaximumSteps(environment.configurations.get<unsigned int>("ChiralCondensate::inverter_max_steps"));
		gmresr->setPrecision(environment.configurations.get<real_t>("ChiralCondensate::inverter_precision"));
	}
	
	bool connected = true;
	
	if (environment.configurations.get<std::string>("ChiralCondensate::measure_condensate_connected") != "true") {
		connected = false;
	}
	
	if (connected && isOutputProcess()) {
		std::cout << "ChiralCondensate::Measuring also the connected part of the chiral susceptibility" << std::endl;
	}
	else if (isOutputProcess()) {
		std::cout << "ChiralCondensate::No measure of the connected part of the chiral susceptibility" << std::endl;
	}

	std::vector< long_real_t > chiralCondensateRe;
	std::vector< long_real_t > chiralCondensateIm;
	std::vector< long_real_t > pseudoCondensateRe;
	std::vector< long_real_t > pseudoCondensateIm;
	std::vector< long_real_t > chiralCondensateConnectedRe;
	std::vector< long_real_t > chiralCondensateConnectedIm;

	std::vector< long_real_t > pionNormRe;
	std::vector< long_real_t > pionNormIm;

	for (unsigned int step = 0; step < max_step; ++step) {
		this->generateRandomNoise(randomNoise);
		
		gmresr->solve(diracOperator, randomNoise, tmp);
		
		std::complex<long_real_t> condensate = AlgebraUtils::dot(randomNoise, tmp);
		chiralCondensateRe.push_back(real(condensate)/volume);
		chiralCondensateIm.push_back(imag(condensate)/volume);
		
		std::complex<long_real_t> pseudoCondensate = AlgebraUtils::gamma5dot(randomNoise, tmp);
		pseudoCondensateRe.push_back(real(pseudoCondensate)/volume);
		pseudoCondensateIm.push_back(imag(pseudoCondensate)/volume);

		std::complex<long_real_t> pionNorm = AlgebraUtils::dot(tmp, tmp);
		pionNormRe.push_back(real(pionNorm)/volume);
		pionNormIm.push_back(imag(pionNorm)/volume);
		
		if (connected) {
			gmresr->solve(diracOperator, tmp, tmp_square);

			std::complex<long_real_t> condensateConnected = AlgebraUtils::dot(randomNoise, tmp_square);
			chiralCondensateConnectedRe.push_back(real(condensateConnected)/volume);
			chiralCondensateConnectedIm.push_back(imag(condensateConnected)/volume);
		}
	}

	if (isOutputProcess() && environment.measurement) {
		GlobalOutput* output = GlobalOutput::getInstance();
		output->push("condensate");
		output->push("pseudocondensate");
		output->push("condensate_connected");
		output->push("pion_norm");

		std::cout << "ChiralCondensate::Chiral condensate is (re) " << this->mean(chiralCondensateRe) << " +/- " << this->standardDeviation(chiralCondensateRe) << std::endl;
		std::cout << "ChiralCondensate::Chiral condensate is (im) " << this->mean(chiralCondensateIm) << " +/- " << this->standardDeviation(chiralCondensateIm) << std::endl;
		
		std::cout << "ChiralCondensate::Pseudo condensate is (re) " << this->mean(pseudoCondensateRe) << " +/- " << this->standardDeviation(pseudoCondensateRe) << std::endl;
		std::cout << "ChiralCondensate::Pseudo condensate is (im) " << this->mean(pseudoCondensateIm) << " +/- " << this->standardDeviation(pseudoCondensateIm) << std::endl;

		std::cout << "ChiralCondensate::Connected chiral condensate susceptibility is (re) " << this->mean(chiralCondensateConnectedRe) << " +/- " << this->standardDeviation(chiralCondensateConnectedRe) << std::endl;
		std::cout << "ChiralCondensate::Connected chiral condensate susceptibility is (im) " << this->mean(chiralCondensateConnectedIm) << " +/- " << this->standardDeviation(chiralCondensateConnectedIm) << std::endl;

		std::cout << "ChiralCondensate::Pion norm is (re) " << this->mean(pionNormRe) << " +/- " << this->standardDeviation(pionNormRe) << std::endl;
		std::cout << "ChiralCondensate::Pion norm is (im) " << this->mean(pionNormIm) << " +/- " << this->standardDeviation(pionNormIm) << std::endl;

		output->write("condensate", this->mean(chiralCondensateRe));
		output->write("condensate", this->mean(chiralCondensateIm));
		
		output->write("pseudocondensate", this->mean(pseudoCondensateRe));
		output->write("pseudocondensate", this->mean(pseudoCondensateIm));

		if (connected) {
			output->write("condensate_connected", this->mean(chiralCondensateConnectedRe));
			output->write("condensate_connected", this->mean(chiralCondensateConnectedIm));
		}

		output->write("pion_norm", this->mean(pionNormRe));
		output->write("pion_norm", this->mean(pionNormIm));

		output->pop("condensate");
		output->pop("pseudocondensate");
		output->pop("condensate_connected");
		output->pop("pion_norm");
	}
	
}

void ChiralCondensate::registerParameters(po::options_description& desc) {
	desc.add_options()
		("ChiralCondensate::number_stochastic_estimators", po::value<unsigned int>()->default_value(20), "The number of stochastic estimators to be used")
		("ChiralCondensate::inverter_precision", po::value<double>()->default_value(0.0000000001), "set the precision used by the inverter")
		("ChiralCondensate::inverter_max_steps", po::value<unsigned int>()->default_value(5000), "set the maximum steps used by the inverter")
		("ChiralCondensate::measure_condensate_connected", po::value<std::string>()->default_value("false"), "Should we measure the connected part of the condensate?")
	;
}

} /* namespace Update */
