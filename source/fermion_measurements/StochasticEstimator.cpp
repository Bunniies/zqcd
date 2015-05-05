/*
 * StochasticEstimator.cpp
 *
 *  Created on: Jul 23, 2012
 *      Author: spiem_01
 */

#include "StochasticEstimator.h"
#include <omp.h>

namespace Update {

#ifndef MULTITHREADING
StochasticEstimator::StochasticEstimator() : randomGenerator(RandomSeed::randomSeed()), randomInteger(RandomSeed::getRandomIntegerGenerator(randomGenerator)) { }

StochasticEstimator::StochasticEstimator(const StochasticEstimator& toCopy) : randomGenerator(RandomSeed::randomSeed()), randomInteger(RandomSeed::getRandomIntegerGenerator(randomGenerator)) { }

StochasticEstimator::~StochasticEstimator() { }
#endif
#ifdef MULTITHREADING
StochasticEstimator::StochasticEstimator() {
	randomGenerator = new random_generator_t*[omp_get_max_threads()];
	randomInteger = new random_integer_generator_t*[omp_get_max_threads()];
	for (int i = 0; i < omp_get_max_threads(); ++i) {
		randomGenerator[i] = new random_generator_t(RandomSeed::randomSeed());
		randomInteger[i] = new random_integer_generator_t(RandomSeed::getRandomIntegerGenerator(*randomGenerator[i]));
	}
}

StochasticEstimator::StochasticEstimator(const StochasticEstimator&) {
	randomGenerator = new random_generator_t*[omp_get_max_threads()];
	randomInteger = new random_integer_generator_t*[omp_get_max_threads()];
	for (int i = 0; i < omp_get_max_threads(); ++i) {
		randomGenerator[i] = new random_generator_t(RandomSeed::randomSeed());
		randomInteger[i] = new random_integer_generator_t(RandomSeed::getRandomIntegerGenerator(*randomGenerator[i]));
	}
}

StochasticEstimator::~StochasticEstimator() {
	for (int i = 0; i < omp_get_max_threads(); ++i) {
		delete randomGenerator[i];
		delete randomInteger[i];
	}
	delete[] randomGenerator;
	delete[] randomInteger;
}

#endif

void StochasticEstimator::generateRandomNoise(extended_dirac_vector_t& vector) {
#pragma omp parallel for
	for (int site = 0; site < vector.localsize; ++site) {
		for (unsigned int mu = 0; mu < 4; ++mu) {
			for (int i = 0; i < diracVectorLength; ++i) {
#ifndef MULTITHREADING
				real_t realPart = (randomInteger() == 0 ? -1 : 1);
#endif
#ifdef MULTITHREADING
				real_t realPart = ((*randomInteger[omp_get_thread_num()])() == 0 ? -1 : 1);
#endif
				vector[site][mu][i] = std::complex<real_t>(realPart,0.);
			}
		}
	}
	vector.updateHalo();
}

void StochasticEstimator::generateRandomNoise(extended_dirac_vector_t* vector, int t0) {
	typedef extended_dirac_vector_t::Layout Layout;
#pragma omp parallel for
	for (int site = 0; site < vector[0].localsize; ++site) {
		for (unsigned int mu = 0; mu < 4; ++mu) {
			for (unsigned int nu = 0; nu < 4; ++nu) {
				//Only if t == t0 and nu == mu we produce noise, dilution of the errors
				if (nu == mu && Layout::globalIndexT(site) == t0) {
					for (int i = 0; i < diracVectorLength; ++i) {
#ifndef MULTITHREADING
						real_t realPart = (randomInteger() == 0 ? -1 : 1);
#endif
#ifdef MULTITHREADING
						real_t realPart = ((*randomInteger[omp_get_thread_num()])() == 0 ? -1 : 1);
#endif
						vector[nu][site][mu][i] = std::complex<real_t>(realPart,0.);
					}
				}
				else {
					for (int i = 0; i < diracVectorLength; ++i) {
						vector[nu][site][mu][i] = std::complex<real_t>(0.,0.);
					}
				}
			}
		}
	}
	for (unsigned int mu = 0; mu < 4; ++mu) vector[mu].updateHalo();
}

void StochasticEstimator::generateSource(extended_dirac_vector_t& vector, int alpha, int c) {
	typedef extended_dirac_vector_t::Layout Layout;
#pragma omp parallel for
	for (int site = 0; site < vector.localsize; ++site) {
		for (unsigned int mu = 0; mu < 4; ++mu) set_to_zero(vector[site][mu]);
		if (Layout::globalIndexX(site) == 0 && Layout::globalIndexY(site) == 0 && Layout::globalIndexZ(site) == 0 && Layout::globalIndexT(site) == 0) {
			vector[site][alpha][c] = 1;
		}
	}

	vector.updateHalo();
}

} /* namespace Update */