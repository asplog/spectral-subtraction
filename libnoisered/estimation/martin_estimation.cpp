#include <cmath>
#include <climits>
#include <cfloat>
#include <numeric>
#include <algorithm>
#include <iostream>

#include "subtraction_manager.h"
#include "mathutils/math_util.h"
#include "martin_estimation.h"
using namespace std;

// Note to future self : ban the static keyword in the 9th circle of hell
void MartinEstimation::algo(std::complex<double> *spectrum, int nrf, double *x, double tinc, bool reinit, bool lastcall)
{
	static int subwc;
	static int segment_number;
	static double *yft = nullptr;
	static double *p = nullptr;
	static double *sn2 = nullptr;
	static double *pb = nullptr;
	static double *pminu = nullptr;
	static double *pb2 = nullptr;
	static double *actmin = nullptr;
	static double *actminsub = nullptr;

	static double *ah = nullptr;
	static double *b = nullptr;
	static double *qeqi = nullptr;
	static double *bmind = nullptr;
	static double *bminv = nullptr;
	static double *lmin = nullptr;
	static double *qisq = nullptr;
	static bool *kmod = nullptr;
	static bool *lminflag = nullptr;
	static double **actbuf = nullptr;

	static MartinEstimation::MartinNoiseParams qq;

	static int nu;
	static int ibuf;
	static double ac;
	static double aca;
	static double acmax;
	static double amax;
	static double aminh;
	static double bmax;
	static double snrexp;
	static double nv, nd;
	static double md, hd, mv, hv;
	static double qeqimax;
	static double qeqimin;
	static double nsms[4];

	if(lastcall)
	{
		delete[] ah;
		delete[] b;
		delete[] qeqi;
		delete[] bmind;
		delete[] bminv;
		delete[] lmin;
		delete[] qisq;
		delete[] kmod;

		delete[] yft;
		delete[] p;
		delete[] sn2;
		delete[] pb;
		delete[] pminu;
		delete[] pb2;
		delete[] lminflag;
		delete[] actmin;
		delete[] actminsub;

		if (actbuf)
		{
			for (int i = 0; i < nu; ++i)
			{
				delete[] actbuf[i];
			}
			delete[] actbuf;
		}

		return;
	}

	// Initialisation
	if (reinit)
	{
		segment_number = 0;
		ibuf = 0;
		qq.taca = 0.0449;    // smoothing time constant for alpha_c = -tinc/log(0.7) in equ (11)
		qq.tamax = 0.392;    // max smoothing time constant in (3) = -tinc/log(0.96)
		qq.taminh = 0.0133;    // min smoothing time constant (upper limit) in (3) = -tinc/log(0.3)
		qq.tpfall = 0.064;   // time constant for P to fall (12)
		qq.tbmax = 0.0717;   // max smoothing time constant in (20) = -tinc/log(0.8)
		qq.qeqmin = 2.;       // minimum value of Qeq (23)
		qq.qeqmax = 14.;      // max value of Qeq per frame
		qq.av = 2.12;             // fudge factor for bc calculation (23 + 13 lines)
		qq.td = 1.536;       // time to take minimum over
		qq.nu = 8;           // number of subwindows
		qq.qith[0] = 0.03;
		qq.qith[1] = 0.05;
		qq.qith[2] = 0.06;
		qq.qith[3] = INT_MAX;
		qq.nsmdb[0] = 47.;
		qq.nsmdb[1] = 31.4;
		qq.nsmdb[2] = 15.7;
		qq.nsmdb[3] = 4.1;

		nu      = qq.nu;
		ac      = 1;
		aca     = exp(-tinc / qq.taca); // smoothing constant for alpha_c in equ (11) = 0.7
		acmax   = aca;          // min value of alpha_c = 0.7 in equ (11) also = 0.7
		amax    = exp(-tinc / qq.tamax); // max smoothing constant in (3) = 0.96
		aminh   = exp(-tinc / qq.taminh); // min smoothing constant (upper limit) in (3) = 0.3
		bmax    = exp(-tinc / qq.tbmax); // max smoothing constant in (20) = 0.8
		snrexp  = -tinc / qq.tpfall;
		nv      = round(qq.td / (tinc * qq.nu));    // length of each subwindow in frames
		if (nv < 4)
		{
			// algorithm doesn't work for miniscule frames
			nv = 4;
			nu = (int) max(round(qq.td / (tinc * nv)), 1.);
		}
		subwc = (int) nv;


		nd = nu * nv;           // length of total window in frames

		MartinEstimation::mh_values(nd, &md, &hd);
		MartinEstimation::mh_values(nv, &mv, &hv);

		for (int i = 0; i < 4; ++i)
			nsms[i] = pow(10., qq.nsmdb[i] * nv * tinc / 10.);  // [8 4 2 1.2] in paper

		qeqimax = 1. / qq.qeqmin;  // maximum value of Qeq inverse (23)
		qeqimin = 1. / qq.qeqmax; // minumum value of Qeq per frame inverse

		if (ah) delete[] ah;
		ah = new double[nrf];
		if (b) delete[] b;
		b = new double[nrf];
		if (qeqi) delete[] qeqi;
		qeqi = new double[nrf];
		if (bmind) delete[] bmind;
		bmind = new double[nrf];
		if (bminv) delete[] bminv;
		bminv = new double[nrf];
		if (lmin) delete[] lmin;
		lmin = new double[nrf];
		if (qisq) delete[] qisq;
		qisq = new double[nrf];
		if (kmod) delete[] kmod;
		kmod = new bool[nrf];

		if (yft) delete[] yft;
		yft = new double[nrf];
		if (p) delete[] p;
		p = new double[nrf];
		if (sn2) delete[] sn2;
		sn2 = new double[nrf];
		if (pb) delete[] pb;
		pb = new double[nrf];
		if (pminu) delete[] pminu;
		pminu = new double[nrf];
		if (pb2) delete[] pb2;
		pb2 = new double[nrf];
		if (lminflag) delete[] lminflag;
		lminflag = new bool[nrf];
		if (actmin) delete[] actmin;
		actmin = new double[nrf];
		if (actminsub) delete[] actminsub;
		actminsub = new double[nrf];

		if (actbuf)
		{
			for (int i = 0; i < nu; ++i)
			{
				delete[] actbuf[i];
			}
			delete[] actbuf;
		}
		actbuf = new double*[nu];
		for (int i = 0; i < nu; ++i)
		{
			actbuf[i] = new double[nrf];
			for (int j = 0; j < nrf; ++j)
			{
				actbuf[i][j] = INT_MAX;
			}
		}

		MathUtil::computePowerSpectrum(spectrum, yft, nrf);
		for (int i = 0; i < nrf; ++i)
		{
			p[i] = yft[i];
			sn2[i] = p[i];
			pb[i] = p[i];
			pminu[i] = p[i];
			pb2[i] = p[i] * p[i];
			lminflag[i] = false;
			actmin[i] = INT_MAX;
			actminsub[i] = INT_MAX;
		}
	}
	else
	{
		MathUtil::computePowerSpectrum(spectrum, yft, nrf);
		segment_number++;
	}



	// Main processing
	double acb = 1. / (1. + pow(accumulate(p, p + nrf, 0.) / accumulate(yft, yft + nrf, 0.) - 1., 2));  // alpha_c-bar(t)  (9)
	ac = aca * ac + (1 - aca) * max(acb, acmax);      // alpha_c(t)  (10)
	for (int i = 0; i < nrf; ++i)
	{
		ah[i] = amax * ac * 1. / (1. + pow(p[i] / sn2[i] - 1., 2));    // alpha_hat: smoothing factor per frequency (11)
	}
	double snr = accumulate(p, p + nrf, 0.) / accumulate(sn2, sn2 + nrf, 0.);

	double localmin = min(aminh, pow(snr, snrexp));
	for (int i = 0; i < nrf; ++i)
	{
		ah[i] = max(ah[i], localmin);       // lower limit for alpha_hat (12)
		p[i] = ah[i] * p[i] + (1 - ah[i]) * yft[i];            // smoothed noisy speech power (3)

		b[i] = min(pow(ah[i], 2.), bmax);              // smoothing constant for estimating periodogram variance (22 + 2 lines)
		pb[i] = b[i] * pb[i] + (1 - b[i]) * p[i];            // smoothed periodogram (20)
		pb2[i] = b[i] * pb2[i] + (1 - b[i]) * pow(p[i], 2);     // smoothed periodogram squared (21)

		qeqi[i] = max(min((pb2[i] - pow(pb[i], 2.)) / (2 * pow(sn2[i], 2.)), qeqimax), qeqimin / segment_number); // Qeq inverse (23)
	}

	double qiav = accumulate(qeqi, qeqi + nrf, 0) / nrf;             // Average over all frequencies (23+12 lines) (ignore non-duplication of DC and nyquist terms)
	double bc = 1. + qq.av * sqrt(qiav);             // bias correction factor (23+11 lines)
	for (int i = 0; i < nrf; ++i)
	{
		bmind[i] = 1. + 2. * (nd - 1.) * (1. - md) / (1. / qeqi[i] - 2. * md);      // we use the simplified form (17) instead of (15)
		bminv[i] = 1. + 2. * (nv - 1.) * (1. - mv) / (1. / qeqi[i] - 2. * mv);      // same expression but for sub windows

		kmod[i] = bc * p[i] * bmind[i] < actmin[i];        // Frequency mask for new minimum

		if (kmod[i])
		{
			actmin[i] = bc * p[i] * bmind[i];
			actminsub[i] = bc * p[i] * bminv[i];
		}
	}


	if (subwc > 0 && subwc < nv)             // middle of buffer - allow a local minimum
	{
		for (int i = 0; i < nrf; ++i)
		{
			lminflag[i] |= kmod[i];     // potential local minimum frequency bins
			pminu[i] = min(actminsub[i], pminu[i]);
			sn2[i] = pminu[i];
		}
	}
	else if (subwc >= nv)                    // end of buffer - do a buffer switch
	{
		ibuf = ibuf % nu;       // increment actbuf storage pointer
		for (int i = 0; i < nrf; ++i)
		{
			actbuf[ibuf][i] = actmin[i];        // save sub-window minimum
		}
		// attention, boucle inverse à l'ordre normal de la matrice (on raisonne en "colonnes")
		for (int i = 0; i < nrf; ++i)
		{
			double tmp = 1;
			for (int j = 0; j < nu; ++j)
			{
				tmp = min(tmp, actbuf[j][i]);
			}
			pminu[i] = tmp;
		}

		int tmp_index = -1;
		for (int i = 0; i < 4; ++i)
		{
			if (qiav < qq.qith[i])
			{
				tmp_index = i;
				break;
			}
		}

		double nsm = nsms[tmp_index];           // noise slope max
		for (int i = 0; i < nrf; ++i)
		{
			lmin[i] = lminflag[i] && !kmod && actminsub[i] < nsm * pminu[i] && actminsub[i] > pminu[i];

			if (lmin[i])
			{
				pminu[i] = actminsub[i];
				for (int j = 0; j < nu; ++j)
				{
					actbuf[j][i] = pminu[i];
				}
			}

			lminflag[i] = 0;
			actmin[i] = INT_MAX;
		}
		subwc = 0;
	}
	++subwc;

	for (int i = 0; i < nrf; ++i)
	{
		x[i] = sn2[i];
		qisq[i] = sqrt(qeqi[i]);

		// xs[t][i] = sn2[i] * sqrt(0.266 * (nd + 100 * qisq[i]) * qisq[i] / (1. + 0.005 * nd + 6. / nd) / (0.5 * 1 / qeqi[i] + nd - 1));
	}
}


void MartinEstimation::mh_values(double d, double *m, double *h)
{
	static double dmh[3][18] =
	{
		{1, 2, 5, 8, 10, 15, 20, 30, 40, 60, 80, 120, 140, 160, 180, 220, 260, 300},
		{0, .26, .48, .58, .61, .668, .705, .762, .8, .841, .865, .89, .9, .91, .92, .93, .935, .94},
		{0, 0.15, 0.48, 0.78, 0.98, 1.55, 2, 2.3, 2.52, 3.1, 3.38, 4.15, 4.35, 4.25, 3.9, 4.1, 4.7, 5}
	};
	int ichosen = -1, jchosen = -1;
	for (auto i = 0U; i < 18; ++i)
	{
		if (dmh[0][i] >= d)
		{
			ichosen = i;
			jchosen = i - 1;
			break;
		}
	}
	if (ichosen == -1)
	{
		ichosen = 17;
		jchosen = 17;
	}

	if (d == dmh[0][ichosen])
	{
		*m = dmh[1][ichosen];
		*h = dmh[2][ichosen];
	}
	else
	{
		double qj = std::sqrt(dmh[0][jchosen]);
		double qi = std::sqrt(dmh[0][ichosen]);
		double q  = std::sqrt(d);
		*h = dmh[2][ichosen] + (q - qi) * (dmh[2][jchosen] - dmh[2][ichosen]) / (qj - qi);
		*m = dmh[1][ichosen] + (qi * qj / q - qj) * (dmh[1][jchosen] - dmh[1][ichosen]) / (qi - qj);
	}
}


MartinEstimation::MartinEstimation(SubtractionManager& configuration):
	Estimation(configuration)
{
}

MartinEstimation::~MartinEstimation()
{
	algo(nullptr, 0, nullptr, 0, false, true);
	return;
}

Estimation *MartinEstimation::clone()
{
	return new MartinEstimation(*this);
}



bool MartinEstimation::operator()(std::complex<double> *input_spectrum)
{
	algo(input_spectrum,  conf.spectrumSize(), noise_power, ((double) conf.getFrameIncrement()) / ((double) conf.getSamplingRate()), _reinit, false);
	_reinit = false;
	return true;
}



void MartinEstimation::specific_onFFTSizeUpdate()
{
	_reinit = true;
	return;
}

// reinit
void MartinEstimation::specific_onDataUpdate()
{
	_reinit = true;
	return;

}
