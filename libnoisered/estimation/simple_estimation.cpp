#include <cmath>
#include <functional>

#include "subtraction_manager.h"
#include "simple_estimation.h"
#include "mathutils/math_util.h"

SimpleEstimation::SimpleEstimation(SubtractionManager &configuration):
	Estimation(configuration)
{
}

SimpleEstimation::~SimpleEstimation()
{

}

Estimation *SimpleEstimation::clone()
{
	return new SimpleEstimation(*this);
}

bool SimpleEstimation::operator()(std::complex<double> *input_spectrum)
{
	if (updateNoise(input_spectrum))
	{
		MathUtil::computePowerSpectrum(input_spectrum, noise_power, conf.spectrumSize());
		return true;
	}
	return false;
}

void SimpleEstimation::specific_onDataUpdate()
{
	noise_rms = 100000;
}

void SimpleEstimation::specific_onFFTSizeUpdate()
{

}

bool SimpleEstimation::updateNoise(const std::complex<double> * const in)
{
	// We estimate the RMS power and compare it to previous noise power
	double current_rms = std::sqrt(MathUtil::mapReduce_n(in, conf.spectrumSize(), 0.0, MathUtil::CplxToPower, std::plus<double>()) / conf.spectrumSize());

	//if we find more or less the same power, it might be noise
	if (current_rms <  noise_rms ||
	   (current_rms >= noise_rms && current_rms <= noise_rms * 1.02))
	{
		noise_rms = current_rms;
		return true;
	}
	return false;
}
