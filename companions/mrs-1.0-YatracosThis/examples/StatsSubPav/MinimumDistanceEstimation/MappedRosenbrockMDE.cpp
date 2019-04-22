/*
* Copyright (C) 2007, 2008, 2009 Raazesh Sainudiin
* Copyright (C) 2009 Jennifer Harlow
*
* This file is part of mrs, a C++ class library for statistical set processing.
*
* mrs is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 3 of the License, or (at
* your option) any later version.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FsITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/*! \file
\brief MDE for Mapped Rosenbrock
 */

#include "histall.hpp"  // headers for the histograms
#include "intervalmappedspnode_measurers.hpp" // ordering for pq split
#include "functionestimator_interval.hpp"
#include "piecewise_constant_function.hpp"  

#include "toolz.hpp"

#include "RosenDensityFobj.hpp" // fobj
#include "SmallClasses.hpp"
#include "Fobj.hpp"
#include "FRosenbrock.hpp"
#include "MRSampler.hpp"

#include <vector>
#include <time.h>   // clock and time classes
#include <fstream>  // input and output streams
#include <iostream>

#include <limits> // to use negative infinity

#include "testDenCommon.hpp" // to use density testing tools
#include "testDenTools.hpp"

// to use assert
#include "assert.h"

using namespace cxsc;
using namespace std;
using namespace subpavings;

//====Method to find the top 3 k======//
// return a vector of the top k indices of a
void topk(vector<double> a, vector<int> & indtop, size_t k){
	multimap<double, size_t> m; // mapping from value to its index
	vector<double>::iterator it;
	
	for (it = a.begin(); it != a.end(); ++it)
		m.insert(make_pair(*it, it - a.begin()));
	
	multimap<double, size_t>::iterator itm; // mapping from value to its index
	size_t indx=0;
	double val =0;
	for (itm = m.begin(); itm != m.end(); ++itm){
		//cout << itm->first <<" , "<< itm->second << endl;
		//if (itm->first != val) { 
			indtop.push_back(itm->second);
			indx++;
		//}
		//val = itm->first;
		if ( indx == k) break; 
	}	
	sort(indtop.begin(), indtop.end());
}

//====Methods needed for the Rosenbrock===============//
void
ProduceMRSamples(Fobj & f, int n_boxes, int n_samples, 
                 double Alb, unsigned seed, bool use_f_scale, RSSample& rs_sample) 
{
	//ofstream out ("MRS_Rosenbrock.samples");//file to store the i.i.d samples
	clock_t T1 = clock (), T2, T3;
	// Construct theSampler with the chosen target shape object FTG
	MRSampler theSampler (f, n_boxes, Alb, seed, (use_f_scale == 1));
		
	T2 = clock ();
	double Ptime = (double) (T2 - T1) / CLOCKS_PER_SEC;
	
	//RSSample rs_sample;
	cout << "before Rej..SampleMany \n";
	cout << "n_samples: " << n_samples << endl;
	theSampler.RejectionSampleMany (n_samples, rs_sample);
	cout << "after Rej..SampleMany \n";
	double IntegralEstimate = _double (rs_sample.IntegralEstimate ());
	cout << "rs_sample IU, N, Nrs: " << rs_sample.EnvelopeIntegral << " " 
       << rs_sample.Samples.size() << " " << rs_sample.Samples.size() << endl;
	cout << "RSSampleMany, integral est: " << IntegralEstimate << endl;
	cout << "RSSampleMany mean: \n"; rs_sample.Mean ();
	//rs_sample.Print(out);
	
	cout << "n interval function calls: " << f.get_interval_calls () << endl;
	cout << "n real function calls: " << f.get_real_calls () << endl;
	
	//----------------------------------------------------------------------------
	T3 = clock ();
	double Stime = (double) (T3 - T2) / CLOCKS_PER_SEC;
	cout << "# CPU Time (seconds). Partitioning: " << Ptime << "  Sampling: " 
       << Stime << "  Total: " << (Ptime + Stime) << endl;
	cout << "# CPU time (secods) per estimate: " 
       << (Ptime + Stime) / (double) (n_samples) << endl;
}
//==========End of methods needed for Rosenbrock==========//

int main(int argc, char* argv[])
{
	// User-defined parameters------------------//
	if ( argc < 7 ) {
		cerr << "Syntax: " << argv[0] << 
		" dataSeed d n maxLeavesEst critLeaves maxCheck" << endl;
		throw std::runtime_error("Syntax: " + std::string(argv[0]) + "data seed, d, n, maxLeavesEst, critLeaves, num_checks, num_iters");
	}

	int dataSeed = atoi(argv[1]); // seed for data generation
	int d = atoi(argv[2]);  // dimension
	const int n = atoi(argv[3]);  // number of points to generate
	size_t maxLeavesEst = atoi(argv[4]);  // number of leaves in estimator
	size_t critLeaves = atoi(argv[5]); //maximum number of leaves for PQ to stop splitting 
	int num_checks = atoi(argv[6]); // check k histograms
	size_t num_iters = atoi(argv[7]); // ...to zoom in
	
	cout << argv[0] << " : process id is " << getpid() << std::endl;
	// End of user-defined parameters--------//
 
	// string formatting for output purposes
	ofstream oss;         // ofstream object
  oss << scientific;  // set formatting for input to oss
  oss.precision(10);
  ostringstream stm;
  stm << dataSeed; // index the txt file produced by stm

	//=======generate actual data and get the root box==============//
	// set up a random number generator and use mt19937 for generator	
	ios::sync_with_stdio ();	// call this function so iostream works with stdio
	cout << SetPrecision (20, 15);	// Number of mantissa digits in I/O
	
	// set up a random number generator and use mt19937 for generator
	gsl_rng * r = gsl_rng_alloc (gsl_rng_mt19937); // set up with default seed
	//long unsigned int seed = 1234;
	gsl_rng_set (r, dataSeed); // change the seed
	cout << "Data seed is " << dataSeed << endl;
	
	// set default values
	int N = 1.5*n;
	cout << "---------------------" << N << "--------------" << endl;
	int n_dimensions = d; 
	int n_samples = N;
	double Alb = 1.0;// partition until lower bound on Acceptance Prob. is > Alb
	unsigned theSeed = dataSeed;
	
	//Parameters specific to the Rosenbrock target
	real Tinverse = 1.0;
	real Height = 100.0;
	real RosenDomainLimit = 10.0;
	int n_boxes = 1000000; 	
	cout << "# n_dimensions: " << n_dimensions << "  n_boxes: " << n_boxes 
       << "  n_samples: " << n_samples << "  rng_seed = " << theSeed  
       << endl; //getchar();
	
	bool UseLogPi = false; // log scale won't work naively
	bool use_f_scale = false;
	
	// make the function object
	FRosenbrock FRosen (n_dimensions, 
                      Tinverse, Height, RosenDomainLimit, UseLogPi);
	
	// produce the samples
	RSSample* actualDataPtr = new RSSample; 
	ProduceMRSamples(FRosen, n_boxes, n_samples, 
                   Alb, theSeed, use_f_scale, *actualDataPtr);

	AdaptiveHistogram* actualHist = new AdaptiveHistogram(true); 
	actualHist->insertFromRSSample(*actualDataPtr, NOLOG, 0);
	ivector pavingBoxEst = actualHist->getRootBox();
	delete actualHist;
	delete actualDataPtr;

	//=========end of getting a root box from the actual data=============
	
	//============== make estimate ============//
	cout << "\nMake the function estimator to " << maxLeavesEst << " leaves" << endl;
	
	// specify function object (from /examples/MappedTargets
	RosenDensityFobj fobj;
	
	/* function estimate is going to use same box as the histograms */
	//ivector pavingBoxEst(d);
	//for(int k=1; k <= d; k++) pavingBoxEst[k] = pavingInterval;
	
	// Use fobj and pavingBox to get an estimator
	FunctionEstimatorInterval estimator(pavingBoxEst, fobj);
	
	LOGGING_LEVEL logEst = NOLOG; // logging for making estimator
	
	#if(1)
	size_t maxLeavesEstDown = static_cast<size_t>(1.2*maxLeavesEst); // go down to 1.2 x max
	#endif
	#if(0)
		size_t maxLeavesEstDown = maxLeavesEst;
	#endif
	
	cout << "pq down to max leaves " << maxLeavesEstDown << endl;
	
	clock_t startEst = clock();
	
	// priority split driven by splitting leaf with max reimann diff
	ReimannDiffMeasurer measurer;
	estimator.prioritySplit(measurer, maxLeavesEstDown, logEst);
			
	// stop recording time here
	clock_t endEst = clock();
	cout << "Number of leaves in estimate: " << estimator.getRootLeaves() << " s."<< endl;	
	cout << "After split, getTotalAreaOfIntervalBand() = "
		<< estimator.getTotalAreaOfIntervalBand() << endl;
	double timingEst1 = ((static_cast<double>(endEst - startEst)) / CLOCKS_PER_SEC);
	cout << "Computing time for pq split in estimate: " << timingEst1 << " s."<< endl;
	startEst = clock();
	
	#if(1) 
		cout << "Hull propagation" << endl;
		estimator.hullPropagation();
		
		cout << "Priority merge to " << maxLeavesEst << " leaves" << endl;
		#if(0)
		// priority merge driven by minimising increase the reimann diff
		estimator.priorityMergeOnLoss(maxLeavesEst, logEst);
		#endif
		#if(1)
		// priority merge driven by merging cherry with minimum reimann diff
		estimator.priorityMerge(maxLeavesEst, logEst);
		#endif
					
		// stop recording time here
		endEst = clock();	
		double timingEst2 = ((static_cast<double>(endEst - startEst)) / CLOCKS_PER_SEC);
		cout << "Computing time for hull propagate and merge up in estimate: " << timingEst2 << " s."<< endl;
		
		cout << "After propagation and priority merge, getTotalAreaOfIntervalBand() = " 
					<< estimator.getTotalAreaOfIntervalBand() << endl;
		cout << "number of leaves is = " << estimator.getRootLeaves() << endl;
	#endif
	
	cout << "Making estimate and normalising" << endl;
	// Make PiecewiseConstantFunction estimate from estimator
	PiecewiseConstantFunction estimate = estimator.makePiecewiseConstantFunction();
	
	cout << "estimate has integral " << estimate.getTotalIntegral() << " before normalizing" << endl;
	real before = estimate.getTotalIntegral();
	
	estimate.normalise();
	
	cout << "estimate has integral " << estimate.getTotalIntegral() << endl;
	
	//optional
	/*
	estimate.outputToTxtTabs("PCF.txt");
	string Integral = "Integral.txt";
	oss.open(Integral.c_str());
	oss << before << "\t" << estimate.getTotalIntegral() << endl;
	oss << flush;
	oss.close();
	*/
	//===========end of estimating function using PCF=========================//
	
		// Use PiecewiseConstantFunction to generate data, supplying our own rng---//
	cout << "\nGenerating data for simulation" << endl;

	RVecData* theDataPtr = new RVecData;   // a container for all the points generated

	// start clock to record time taken to simulate data
	clock_t startData = clock();

	estimate.simulateData(*theDataPtr, N, r);

	// stop recording time here
	clock_t endData = clock();	
	double timingData = ((static_cast<double>(endData - startData)) / CLOCKS_PER_SEC);
	cout << "Computing time for simulating data: " << timingData << " s."<< endl;

	cout << (*theDataPtr).size() << " points generated" << endl;

	// optional - remove comments to output simulated data 
	/*
	string dataFileName = "SimulatedData";
	dataFileName += stm.str(); 
	dataFileName += ".txt"; 
	oss.open(dataFileName.c_str());
	for (size_t i = 0; i < n; i++) { 
		for (size_t j = 1; j <= d; j++) {
				oss << (*theDataPtr)[i][j] << "\t";
		}
		oss << "\n";
		//cout << "\n";
	}
	oss << flush;
	oss.close();
	cout << "Simulated data written to  " << dataFileName << endl;
	*/
	// End of generating data--------//
	
	// Minimum distance estimation with hold-out--------//
	cout << "\nRunning minimum distance estimation with hold-out..." << endl;
	
	int trainCount = n; 
	int holdOutCount = N-n;
	cout << n << " training data and " << holdOutCount << " validation data inserted." << endl; 

	// parameters for function insertRVectorForHoldOut()
	SplitNever sn; 

	// parameters for prioritySplitAndEstimate
	CompCountVal compCount; 
	CritLeaves_GTEV he(critLeaves); //the PQ will stop after critLeaves are reached
	size_t minChildPoints = 0;
	size_t maxLeafNodes = 1000000; 
	bool computeIAE = FALSE; // do not compute the IAE first

	vector<int> sequence; //to store all the thetas
	size_t startLeaves = 0; 
	sequence.push_back(startLeaves + 1);
	sequence.push_back(critLeaves);
	
	//sequence to be used
	int increment = (critLeaves-startLeaves)/(num_checks);
	cout << "Increment by : " << increment << endl;
	int temp = startLeaves;
	while ( temp < critLeaves) {
		temp += increment;
	 	sequence.push_back(temp); 
	 }
	sort(sequence.begin(), sequence.end());
	sequence.erase( unique( sequence.begin(), sequence.end() ), sequence.end() );
	//for ( vector<int>::iterator it = sequence.begin(); it != sequence.end(); it++)
		//cout << *it << endl;
		
	cout << "Perform " << num_iters << " iterations" << endl; 
	vector<double>* vecMaxDelta = new vector<double>;	//which delta vector is this?
	vector<real>* vecIAE = new vector<real>;		//which IAE is this?
	
	size_t k = 3; //take the best three  delta - what is best?

	// start the clock here for ...?
	double timing = 0;
	clock_t start, end;
	start = clock();

	size_t iters = 0;

	while ( (increment) > 1 && iters < num_iters && (critLeaves - startLeaves) > num_checks) {				
		cout << "\nIteration " << iters << "......" << endl;

		// insert simulated data into an AdaptiveHistogramValidation object
 		AdaptiveHistogramValidation myHistVal(pavingBoxEst);
 		myHistVal.insertFromRVecForHoldOut(*theDataPtr, sn, holdOutCount, NOLOG);
			
	 	//run MDE
	 	myHistVal.prioritySplitAndEstimate
	 					(compCount, he, NOLOG, 
	 					minChildPoints, 0.0, estimate, 
	 					maxLeafNodes, computeIAE, sequence,	
	 					*vecMaxDelta, *vecIAE); //don't compute vecIAE here - is it possible?
	
		//get the best 3 delta max values			
		vector<int> indtop;
		topk(*vecMaxDelta, indtop, 3);
		(*vecMaxDelta).clear();
		(*vecIAE).clear();
		//cout << "Best three indices: " << endl;
		//for ( vector<int>::iterator it = indtop.begin(); it != indtop.end(); it++)
			// //*it = position //*sequence[*it] = leaves
		 	//{ cout << *it << "\t" << sequence[*it] << endl;}
		
		//update final_sequence
		startLeaves = sequence[indtop[0]];
		critLeaves = sequence[indtop[2]];
		if ( (critLeaves - startLeaves) < num_checks ) 
		{ num_checks = critLeaves - startLeaves; }
		
		increment = (critLeaves-startLeaves)/(num_checks);
		//cout << " Increment by: " << increment << endl;
		
		temp = startLeaves;
		while ( temp < critLeaves) {
			temp += increment;
			//cout << "temp: "<< temp << endl;
			sequence.push_back(temp); 
		}
		sort(sequence.begin(), sequence.end());
		sequence.erase( unique( sequence.begin(), sequence.end() ), sequence.end() );
		
		//cout << "updated sequence: " << endl;
		//for ( vector<int>::iterator it = sequence.begin(); it != sequence.end(); it++)
			//cout << *it << endl;	
				
		//increment iters
		iters++;
	 } //end of while loop

	//Run MDE with the final sequence after breaking out of the loop	
	cout << "\nRun MDE with the final sequence..." << endl;
	AdaptiveHistogramValidation myHistVal(pavingBoxEst);
	myHistVal.insertFromRVecForHoldOut(*theDataPtr, sn, holdOutCount, NOLOG);

	computeIAE = TRUE;
	myHistVal.prioritySplitAndEstimate
	 				(compCount, he, NOLOG, 
	 				minChildPoints, 0.0, estimate, 
	 				maxLeafNodes, computeIAE, sequence,	
	 				*vecMaxDelta, *vecIAE);
						
	end = clock();
	timing = ((static_cast<double>(end - start)) / CLOCKS_PER_SEC);
	cout << "Computing time for MDE: " << timing << " s."<< endl;
		
	//find the minimum delta
	double minDelta = *min_element((*vecMaxDelta).begin(), (*vecMaxDelta).end());	

	//find the position of the minimum delta
	size_t minPos = min_element((*vecMaxDelta).begin(), (*vecMaxDelta).end()) - (*vecMaxDelta).begin();
	int numLeavesDelta = sequence[minPos];
		
	//get the IAE using vecIAE
	real IAEforMinDelta = (*vecIAE)[numLeavesDelta - 1];
		
	// get minimum IAE
	real minIAE = *min_element((*vecIAE).begin(), (*vecIAE).end());
		
	//find the position of the minimum IAE	
	int numLeavesIAE = min_element((*vecIAE).begin(), (*vecIAE).end()) - (*vecIAE).begin() + 1;

	//delete pointers;
	delete vecIAE;
	delete vecMaxDelta;	
	delete theDataPtr;

	try {
		gsl_rng_free (r);
		r = NULL;
	}
	catch(...) {}// catch and swallow

	// output results to txt file
	cout << IAEforMinDelta << "\t" << numLeavesDelta << "\t" << minIAE << "\t" << numLeavesIAE << endl;
	string outputName;
	outputName = "results";
	outputName += stm.str();
	outputName += ".txt";
	oss.open(outputName.c_str());
	oss << IAEforMinDelta << "\t" << numLeavesDelta << "\t" << minIAE << "\t" << numLeavesIAE << endl;
	oss << flush;
	oss.close();
	cout << "Error computations output to " << outputName << endl;
	
	// optional - remove comments to output ...
	/*
	string sequenceName;
	sequenceName = "Sequence";
	sequenceName += stm.str();
	sequenceName += ".txt";
	oss.open(sequenceName.c_str());
	for (size_t i = 0; i < (final_sequence).size(); i++){
		oss << (final_sequence)[i] << endl;
	}			 
	oss << flush;
	oss.close();
	*/	
		
	return 0;


} // end of MDE test program