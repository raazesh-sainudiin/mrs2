/*
* Copyright (C) 2012 Jennifer Harlow
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
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/*! \file
\brief PCFV definitions
*/

#include "pcfval.hpp"
#include "adaptivehistogramvalidation.hpp"
#include "subpaving_exception.hpp"

#include <iostream> // to use standard input and output
#include <string>   // to use the C++ string class
#include <fstream>  // for ifstream, ofstream
#include <sstream>  // to be able to manipulate strings as streams
#include <stdexcept> // use exceptions
#include <cassert>

#include <gsl/gsl_randist.h> // to use gsl_ran_discrete_preproc


//#define DEBUGCOVERAGEREGION

using namespace subpavings;
using namespace std;



// -------------------implementation of PCFV class --------------


// ----------- public methods

// no-argument constructor 
PCFV::PCFV()
         : rootPaving(NULL), label(0)
{}

// initialised constructor 
PCFV::PCFV(
										const ivector& v,
										int lab)
         : rootPaving(NULL), label(lab)
{
    try {
        // check the box here
        if (!checkBox(v)) {
			throw subpavings::MalconstructedBox_Error(
			"PCFV::PCFV(const ivector&, int lab)");
		}
        rootPaving = new RealMappedSPnode(v, real(0.0)); 
		
	}
    catch (exception const& e) {
		constructor_error_handler();
    }
}

PCFV::PCFV(
								const RealMappedSPnode& rmspn, 
								int lab)
         : rootPaving(NULL), label(lab)
{
    try {
        // check spn has box
		if (rmspn.isEmpty()) {
			throw subpavings::NoBox_Error(
			"PCFV::PCFV(const RealMappedSPnode&, int lab)");
		}
        
		rootPaving = new RealMappedSPnode(rmspn);
	}
    catch (exception const& e) {
		constructor_error_handler();
    }
}

PCFV::PCFV(const AdaptiveHistogramValidation& adh)
         : rootPaving(NULL), label(0)
{
	try {
        // check adh has paving
		if (!adh.hasSubPaving()) {
			throw subpavings::NullSubpavingPointer_Error(
			"PCFV::PCFV(const AdaptiveHistogramValidation&)");
		}
        rootPaving = new RealMappedSPnode(*adh.getSubPaving());
		label = adh.getLabel();
		
		_normalise();
	}
    catch (exception const& e) {
		constructor_error_handler();
    }
	
}

// copy constructor*/
PCFV::PCFV(
								const PCFV& other)
        : rootPaving(NULL), label(other.label)
{
    try {
		
		if (other.hasSubPaving()) {
			rootPaving = new RealMappedSPnode(*(other.getSubPaving()));
			
		} // else subpaving is NULL
		else {
			throw NullSubpavingPointer_Error(
				"PCFV::PCFV(const PCFV&)");
		}
		
	}
    catch (exception const& e) {
		constructor_error_handler();
	}

}


//Destructor
PCFV::~PCFV()
{
	try {
		delete rootPaving;
		rootPaving = NULL;

	}
	catch (exception const& e) {
		try {
			constructor_error_handler();
		}
		catch(std::exception const& ee) {
			std::cerr << "Error in PCFV destructor:\n" << ee.what() << std::endl;
		}
	} // exceptions ultimately swallowed
}

// assignment operator
PCFV& PCFV::operator=(
    PCFV rhs)
{
	rhs.swap(*this);
	return *this;

}


int PCFV::getLabel() const
{return label;}

void PCFV::setLabel(int lab)
{label = lab;}

const RealMappedSPnode PCFV::getCopySubPaving() const
{
	if (!hasSubPaving()) {
		throw NullSubpavingPointer_Error(
					"PCFV::getCopySubpaving()");
	}
	return RealMappedSPnode(*getSubPaving());
}

// get whether this has a subpaving.
bool PCFV::hasSubPaving() const
{
    return ( getSubPaving() != NULL );
}

bool PCFV::hasNegativePiecewiseConstantValues() const
{
	bool result = false;
	if (hasSubPaving()) result = getSubPaving()->hasNegativeRangeInTree();
	return result;
}

bool PCFV::hasInfinitePiecewiseConstantValues() const
{
	bool result = false;
	if (hasSubPaving()) result = getSubPaving()->hasInfiniteRangeInTree();
	return result;
}

cxsc::ivector PCFV::getRootBox() const
{
	if (!hasSubPaving()) {
		throw NullSubpavingPointer_Error(
					"PCFV::getRootBox()");
	}
	return getSubPaving()->getBox();
}



int PCFV::getDimensions() const
{
	int retValue = 0;
	if (hasSubPaving()) {
		retValue = getSubPaving()->getDimension();
	}
	return retValue;
}

cxsc::real PCFV::getDomainVolume() const
{
	real retValue(0.0);
	if (hasSubPaving()) {
		retValue = getSubPaving()->nodeRealVolume();
	}
	return retValue;
}
	
	
// Gets number of leaf nodes in the root paving.
size_t PCFV::getRootLeaves() const
{
	size_t result = 0;
	if (hasSubPaving()) {
		result = getSubPaving()->getNumberLeaves();
	}
	return result;
}

// returns a vector of leaf levels as ints
// left to right, 0 is root
IntVec PCFV::getLeafLevels() const
{
    IntVec levels; // empty container

    if (hasSubPaving()) {
        getSubPaving()->getLeafNodeLevels(levels, 0);
        //levels has now been filled in
    }
    return levels;
}


// Get a string of the leaf node levels.
std::string PCFV::getLeafLevelsString() const
{
    string retValue = "";
    if (hasSubPaving())
        retValue = getSubPaving()->getLeafNodeLevelsString();

    return retValue;
}




//splits this according to string instruction
//returns true if some splitting was achieved
bool PCFV::splitToShape(std::string instruction)
{
	
	// checks:  is there a root paving, is the string properly formed?
	if (!hasSubPaving()) {
		throw NullSubpavingPointer_Error(
				"PCFV::splitToShape()");
	}
	bool success = false;
	RealMappedSPnode temp(*getSubPaving()); // copy to temp
	try {
		if (instruction.length() == 0) {
			throw std::invalid_argument(
				"PCFV::splitToShape() : No instruction");
		}

		std::string legal(", 0123456789");
		if (instruction.find_first_not_of(legal) != std::string::npos) {
			throw std::invalid_argument(
				"PCFV::splitToShape() : Illegal character");
		}

		// all seems to be okay, we can start splitting the root paving
		
		success = getSubPaving()->splitRootToShape(instruction);
		
		if (!success) {
			
			handleSPError(temp);
	   }
	   
	}
	catch (std::invalid_argument const& ia) {
		cerr << ia.what() << endl;
		handleSPError(temp);
		success = false;
	}
	catch (std::logic_error const& le) {
		cerr << le.what() << endl;
		handleSPError(temp);
		success = false;
	}
	return success;
	// any other exceptions are unhandled
}



void PCFV::allocateRanges(
				const std::vector< cxsc::real >& rangesToAllocate)
{
	// checks:  is there a root paving, is the string properly formed?
	if (!hasSubPaving()) {
		throw NullSubpavingPointer_Error(
				"PCFV::allocateRanges(...)");
	}
	RealMappedSPnode temp(*getSubPaving()); // copy to temp
	try {
		getSubPaving()->allocateRanges(rangesToAllocate);
	   
	}
	catch (std::invalid_argument const& ia) {
		cerr << ia.what() << endl;
		handleSPError(temp);
	}
	
	// any other exceptions are unhandled
}

void PCFV::reshapeToUnion(
					const PCFV& other)
{
	if ( !hasSubPaving() || !other.hasSubPaving() ) {
		throw NullSubpavingPointer_Error(
				"PCFV::reshapeToUnion(const PCFV&)");
	}
	
	getSubPaving()->reshapeToUnion(*other.getSubPaving());
}

PCFV PCFV::makeShapeToUnionJ(
					const PCFV& other) const
{
	if ( !hasSubPaving() || !other.hasSubPaving() ) {
		throw NullSubpavingPointer_Error(
				"PCFV::reshapeToUnion(const PCFV&)");
	}
	
	PCFV tmp(*this);
	tmp.reshapeToUnion(other);
	return tmp;
}


// NEW JUNE 2012 for log posteriors
/* return value could be cxsc::SignalingNan (if -ve pieces where
 * there are points) or else -cxsc::Infinity (if 0 pieces where there
 * are points) or else else +cxsc::Infinity (if infinite pieces where there
 * are points).
 * Using cxsc::exp(-cxsc::Infinity) will give 0 but using 
 * cxsc::exp(cxsc::Infinity) and cxsc::exp(cxsc::SignalingNaN)
 * will give an error.  Both -cxsc::Infinity
 * and cxsc::Infinity will give cxsc::IsInfinity() true.*/
cxsc::real PCFV::getLogLikelihood(
					const AdaptiveHistogramValidation& adh) const
{
	if ( !hasSubPaving() || !adh.hasSubPaving() ) {
		throw NullSubpavingPointer_Error(
			"PCFV::getLogLikelihood(const AdaptiveHistogram&)");
	}
	return getSubPaving()->getLogLikelihood(*(adh.getSubPaving()));
}


cxsc::real PCFV::getTotalIntegral() const
{
	if (!hasSubPaving()) {
		throw NullSubpavingPointer_Error(
		"PCFV::getTotalIntegral()");
	}
	return getSubPaving()->getTotalAbsLeafAreaRangeWithBox();
}

cxsc::real PCFV::getIAE(
						const PCFV& pcf) const
{
	if (!hasSubPaving() || !pcf.hasSubPaving()) {
		throw NullSubpavingPointer_Error(
		"PCFV::getIAE()");
	}
	return getSubPaving()->getTotalAbsDiffLeafAreaRangeWithBox(
									*(pcf.getSubPaving()) );
}

cxsc::real PCFV::getMaxPiecewiseConstant() const
{
	if (!hasSubPaving()) {
		throw NullSubpavingPointer_Error(
		"PCFV::getMaxPiecewiseConstant()");
	}
	return getSubPaving()->getMaxRangeForLeavesInTree();
}

/* Addition to self operator.*/     
PCFV& PCFV::operator+= (
					const PCFV& rhs)
{
	if( !hasSubPaving() || !(rhs.hasSubPaving()) ) {
		throw subpavings::NullSubpavingPointer_Error(
		"PCFV::operator+= (const PCFV&)");
	}
	
	getSubPaving()->operator+=( *(rhs.getSubPaving()) );
	return *this;
}

/* \brief Addition operator.*/
const PCFV PCFV::operator+ (
					const PCFV& rhs) const
{
	PCFV result =(*this);

	result+= rhs;
	
	if ( label != rhs.getLabel() ) result.setLabel(0);
	
	return result;
}

/* Self-scalar addition operator.*/
PCFV& PCFV::operator+= (
					const cxsc::real& add)
{
	if( !hasSubPaving() ) {
		throw subpavings::NullSubpavingPointer_Error(
		"PCFV::operator+= (const cxsc::real&)");
	}
	
	getSubPaving()->operator+=( add );
	return *this;
}

/* Scalar addition operator*/
const PCFV PCFV::operator+ (
					const cxsc::real& add) const
{
	PCFV result =(*this);

	result+= add;
	
	return result;
}

/*Subtraction from self operator.*/  
PCFV& PCFV::operator-= (
					const PCFV& rhs)
{
	
	if( !hasSubPaving() || !(rhs.hasSubPaving()) ) {
		throw subpavings::NullSubpavingPointer_Error(
		"PCFV::operator-= (const PCFV&)");
	}
	
	getSubPaving()->operator-=( *(rhs.getSubPaving()) );
	return *this;
}

/* Subtraction operator.*/   
const PCFV PCFV::operator- (
					const PCFV& rhs) const
{
	PCFV result =(*this);

	result-= rhs;

	if ( label != rhs.getLabel() ) result.setLabel(0);
	
	return result;
}

/* Self-scalar subtraction operator.*/
PCFV& PCFV::operator-= (
					const cxsc::real& sub)
{
	if( !hasSubPaving() ) {
		throw subpavings::NullSubpavingPointer_Error(
		"PCFV::operator-= (const cxsc::real&)");
	}
	
	getSubPaving()->operator-=( sub );
	return *this;
}

/* Scalar subtraction operator*/
const PCFV PCFV::operator- (
					const cxsc::real& sub) const
{
	PCFV result =(*this);

	result-= sub;
	
	return result;
}

/* Multiplication of self operator.*/
PCFV& PCFV::operator*= (
					const PCFV& rhs)
{
	
	if( !hasSubPaving() || !(rhs.hasSubPaving()) ) {
		throw subpavings::NullSubpavingPointer_Error(
		"PCFV::operator*= (const PCFV&)");
	}
	
	getSubPaving()->operator*=( *(rhs.getSubPaving()) );
	return *this;
}

/* Multiplication operator.*/   
const PCFV PCFV::operator* (
					const PCFV& rhs) const
{
	PCFV result =(*this);

	result*= rhs;
	
	if ( label != rhs.getLabel() ) result.setLabel(0);
	
	return result;
}

/* Self-scalar multiplication operator.*/
PCFV& PCFV::operator*= (
					const cxsc::real& mult)
{
	if( !hasSubPaving() ) {
		throw subpavings::NullSubpavingPointer_Error(
		"PCFV::operator*= (const cxsc::real&)");
	}
	
	getSubPaving()->operator*=( mult );
	return *this;
}

/* Scalar multiplication operator*/
const PCFV PCFV::operator* (
					const cxsc::real& mult) const
{
	PCFV result =(*this);

	result*= mult;
	
	return result;
}


/* Division of self operator.*/
PCFV& PCFV::operator/= (
					const PCFV& rhs)
{
	if( !hasSubPaving() || !(rhs.hasSubPaving()) ) {
		throw subpavings::NullSubpavingPointer_Error(
		"PCFV::operator/= (const PCFV&)");
	}
	
	getSubPaving()->operator/=( *(rhs.getSubPaving()) );
	return *this;
}

/* Division operator.*/   
const PCFV PCFV::operator/ (
						const PCFV& rhs) const
{
	PCFV result =(*this);

	result/= rhs;

	if ( label != rhs.getLabel() ) result.setLabel(0);
	
	return result;
}

/*Self-scalar division operator.*/
PCFV& PCFV::operator/= (
						const cxsc::real& div)
{
	if( !hasSubPaving() ) {
		throw subpavings::NullSubpavingPointer_Error(
		"PCFV::operator/= (const cxsc::real&)");
	}
	if( div == cxsc::real(0.0) ) {
		throw std::invalid_argument(
		"PCFV::operator/= (const cxsc::real&)");
	}
	
	getSubPaving()->operator/=( div );
	return *this;
}

/*Scalar division operator.*/
const PCFV PCFV::operator/ (
						const cxsc::real& div) const
{
	PCFV result =(*this);

	result/= div;

	return result;
}

// normalising method
void PCFV::normalise()
{
	if (!hasSubPaving()) {
		throw NullSubpavingPointer_Error(
				"PCFV::normalise()");
	}
	if (getTotalIntegral() <= 0.0) {
		throw runtime_error(
			"PCFV::normalise() : integral <= 0.0");
	}
	
	_normalise();
	
}

// normalising method
const PCFV PCFV::makeNormalised() const
{
	if (!hasSubPaving()) {
		throw NullSubpavingPointer_Error(
				"PCFV::makeNormalised()");
	}
	if (getTotalIntegral() <= 0.0) {
		throw runtime_error(
			"PCFV::makeNormalised() : integral <= 0.0");
	}
	
	PCFV temp(*this);
	
	temp._normalise();
	return temp;
}

const PCFV PCFV::makeMarginal(
								const std::vector<int>& reqDims) const
{
	if (!hasSubPaving()) {
		throw NullSubpavingPointer_Error(
				"PCFV::makeMarginal(const std::vector<int>&)");
	}
	PCFV temp(*this);
	
	temp._marginalise(reqDims);
	return temp;
}

const PCFV PCFV::makeSlice(
					const std::vector < int >& sliceDims,
					const std::vector < cxsc::real >& slicePts) const
{
	if (!hasSubPaving()) {
		throw NullSubpavingPointer_Error(
		"PCFV::makeSlice(const std::vector < int >&, const std::vector < cxsc::real >&)");
	}
	return PCFV(
			getSubPaving()->makeSlice(sliceDims, slicePts), getLabel());
}


// do checks and use private coverage method to find coverage
cxsc::real PCFV::findCoverage(const rvector& pt) const
{
	if (!hasSubPaving()) {
		throw NullSubpavingPointer_Error(
					"PCFV::findCoverage(const rvector&)");
	}
	if (getDimensions() != (Ub(pt) - Lb(pt) + 1)) {
		throw IncompatibleDimensions_Error("PCFV::findCoverage(const rvector&)");
	}
	if (hasNegativePiecewiseConstantValues()) {
		throw std::runtime_error(
			"PCFV::findCoverage(const rvector&) : subpaving has an negative range");
	}
	if (getSubPaving()->hasInfiniteRangeInTree()) {
		throw std::runtime_error(
			"PCFV::findCoverage(const rvector&) : subpaving has an infinite range");
	}
	
	return _coverage(pt);
}




// get value mapped to leaf node of subpaving containing \a pt
cxsc::real PCFV::pointwiseExtension(
				const rvector& pt) const
{
	
	if (!hasSubPaving()) {
		throw NullSubpavingPointer_Error(
					"PCFV::pointwiseExtension(const rvector&)");
	}
	if (getDimensions() != (Ub(pt) - Lb(pt) + 1)) {
		throw IncompatibleDimensions_Error(
			"PCFV::pointwiseExtension(const rvector&)");
	}
	cxsc::real result(0.0);
	
	const RealMappedSPnode * container = 
				getSubPaving()->findContainingNode(pt);
				
	if (container != NULL) {
		result = container->getRange();
		
	}
		
	return result;
}

cxsc::real PCFV::getL1Distance(
				const PCFV& other) const
{
	
	if (!hasSubPaving() || !(other.hasSubPaving())) {
		throw NullSubpavingPointer_Error(
		"PCFV::getL1Distance(const PCFV&)");
	}
			
	return getSubPaving()->getL1Distance( *(other.getSubPaving()) );
}



void PCFV::outputCoverageRegion(	std::ostream & os,
											cxsc::real cov,
											int prec) const
{
	// have to use cxsc io manipulators
	os << cxsc::SaveOpt;
	os << cxsc::Variable << cxsc::SetPrecision(prec+2,prec);
	outputCoverageRegion(os, cov);
	os << cxsc::RestoreOpt;

}

void PCFV::outputCoverageRegion(	std::ostream & os,
											cxsc::real cov) const
{
	RealMappedSPnode::ConstPtrs covNodes;
	covNodes = findCoverageRegion(covNodes, cov);

	RealMappedSPnode::ConstPtrsItr it;
	for (it = covNodes.begin(); it < covNodes.end(); ++it) {
		(*it)->leavesOutputTabs(os);
	}
	os << endl;
	
}

void PCFV::outputCoverageRegion(
			const std::string& covFileName,
			cxsc::real cov,
			int prec,
			bool confirm) const
{
	//output covNodes to .txt	
	ofstream os;
	os.open(covFileName.c_str());
	if (os.is_open()) {	
		
		ostringstream oss;
		oss << cxsc::SaveOpt;
		oss << cxsc::Variable << cxsc::SetPrecision(prec+2,prec);
	
		outputCoverageRegion(oss, cov);
		os << (oss.str());
	
	os.close();
	if (confirm) cout << "coverage nodes output to " << covFileName << endl;
	}
	else cout << "Could not open coverage region file " << covFileName << endl;
}

void PCFV::outputCoverageRegion(
			const std::string& covFileName,
			cxsc::real cov,
			bool confirm) const
{
	//output covNodes to .txt	
	ofstream os;
	os.open(covFileName.c_str());
	if (os.is_open()) {	
		
		ostringstream oss;
		outputCoverageRegion(oss, cov);
		os << (oss.str());
	
	os.close();
	if (confirm) cout << "coverage nodes output to " << covFileName << endl;
	}
	else cout << "Could not open coverage region file " << covFileName << endl;
}


RVecData& PCFV::simulateData(RVecData& container, 
					size_t numberToSimulate,
					gsl_rng * r) const
{
	if (!hasSubPaving()) {
		throw NullSubpavingPointer_Error(
		"PCFV::simulateData(RVecData&, size_t, gsl_rng*)");
	}
	
	gsl_ran_discrete_t* gslpdfstruct = NULL;
	RealMappedSPnode::ConstPtrs* leavesPtr = NULL;
	std::vector< cxsc::ivector >* boxesPtr = NULL;
	double* weightsArrayPtr = NULL;
	
	try {/* the weights are the "areas" of the boxes of the 
		 * leaves of the subpaving managed by this, area = range * volume */
		
		//copy container to tmp temporarily
		RVecData tmp(container.begin(), container.end());
		tmp.reserve(container.size() + numberToSimulate);
		 
		leavesPtr = new RealMappedSPnode::ConstPtrs();
		getSubPaving()->getConstLeaves(*leavesPtr);

		size_t sizeWeights = leavesPtr->size();
		
		boxesPtr = new std::vector< cxsc::ivector >();
		boxesPtr->reserve(sizeWeights);
		weightsArrayPtr = new double [sizeWeights];
		
		size_t index = 0;
		for (RealMappedSPnode::ConstPtrsItr it = leavesPtr->begin();
					it < leavesPtr->end();
					++it, ++index) {
			// put box into boxes and weight into weightsArray
			boxesPtr->push_back( (*it)->getBox() );
			weightsArrayPtr[index] = _double((*it)->getRealAreaRangeWithBox());
		}
		
		try {
			delete leavesPtr;
			leavesPtr = NULL;
		}
		catch(...) {}// catch and swallow
		
		gslpdfstruct = gsl_ran_discrete_preproc(sizeWeights, weightsArrayPtr);
		
		int d = getDimensions(); 
		for (size_t i = 0; i < numberToSimulate; ++i) {
			
			rvector thisrv(d);
			size_t proposedIndex = gsl_ran_discrete(r, gslpdfstruct);
			//thisrv = DrawUnifBox(r, boxes[proposedIndex]);
			tmp.push_back( DrawUnifBox(r, boxesPtr->at(proposedIndex)) );
		}  // data  should be in tmp
		
		assert(tmp.size() == container.size() + numberToSimulate);
		
		tmp.swap(container);
	
		try {
			delete boxesPtr;
			boxesPtr = NULL;
		}
		catch(...) {}// catch and swallow
		try {
			delete [] weightsArrayPtr;
			weightsArrayPtr = NULL;
		}
		catch(...) {}// catch and swallow
		
		try {
			gsl_ran_discrete_free (gslpdfstruct);
			gslpdfstruct = NULL;
		}
		catch(...) {}// catch and swallow
		
		return container;
		
	}
	catch(...) {
		try {
			if (gslpdfstruct != NULL) gsl_ran_discrete_free (gslpdfstruct);
			if (leavesPtr !=NULL) delete leavesPtr;
			if (boxesPtr !=NULL) delete boxesPtr;
			if (weightsArrayPtr !=NULL) delete [] weightsArrayPtr;
		}
		catch(...) {}// catch and swallow
		throw; // rethrow original exception
	}
}

RVecData& PCFV::simulateData(RVecData& container, 
					size_t numberToSimulate,
					long unsigned int seed) const
{
	gsl_rng * r = NULL;
	
	try {
		/* make a generator for this mcmc run */
		r = gsl_rng_alloc (gsl_rng_mt19937);
		
		gsl_rng_set(r, seed);

		return simulateData(container, numberToSimulate, r);
		
		try {
			gsl_rng_free(r);
			r = NULL;
		}	
		catch(...) {}// catch and swallow
	}
	catch(...) {
		try {
			if (r != NULL) gsl_rng_free (r);
		}
		catch(...) {}// catch and swallow
		throw; // rethrow original exception
	}
}

RVecData& PCFV::simulateData(RVecData& container, 
					size_t numberToSimulate) const
{
	long unsigned int seed = 1234;
	return simulateData(container, numberToSimulate, seed);
}

// Method to output the subpaving, leaves only
std::ostream & PCFV::outputToStreamTabs(std::ostream & os,
								int prec) const
{
	if (hasSubPaving()) {
		
		// have to use cxsc io manipulators
		os << cxsc::SaveOpt;
		os << cxsc::Variable << cxsc::SetPrecision(prec+2,prec);

		getSubPaving()->leavesOutputTabs(os); // the output
		
		os << cxsc::RestoreOpt;
	}
	
    return os;
}

// Method to output the subpaving to a txt file
// Output goes to file named according to argument s
void PCFV::outputToTxtTabs(const std::string& s,
                            int prec) const
{
	outputToTxtTabs(s, prec, false);
}

// Method to output the subpaving to a txt file
// Output goes to file named according to argument s
void PCFV::outputToTxtTabs(const std::string& s,
                            int prec, bool confirm) const
{

	// To generate a file output of the PCFV object
	ofstream os(s.c_str());         // Filename, c-string version
	if (os.is_open()) {
		
		if (hasSubPaving()) {

			getSubPaving()->leavesOutputTabs(os, prec); // the output
			
		}
		if (confirm)
			std::cout << "The output of the PCFV "
				<< "has been written to " << s << std::endl << std::endl;
		os.close();
	}
	else {
		std::cerr << "Error: could not open file named "
			<< s << std::endl << std::endl;
	}
}



void PCFV::outputRootToTxt(const std::string& s,
										int prec) const
{
	outputRootToTxt(s, prec, false);
}

// Method to output details and stats on the root paving to a txt file
// Output goes to file named according to arguement s
void PCFV::outputRootToTxt(const std::string& s,
										int prec, bool confirm) const
{
 	// To generate a file output of root node of the PCFV
	ofstream os(s.c_str());         // Filename, c-string version
	if (os.is_open()) {
	
		if (hasSubPaving()) {
			
			os << cxsc::SaveOpt;
			os << cxsc::Variable << cxsc::SetPrecision(prec+2,prec);
	
			getSubPaving()->nodePrint(os); // the output
			
			os << cxsc::RestoreOpt;
			
		}
		if (confirm)
			std::cout << "Details of the root paving of the PCFV "
				<< "has been written to " << s << std::endl << std::endl;
				
		os.close();
	}
	else {
		std::cerr << "Error: could not open file named "
			<< s << std::endl << std::endl;
	}
}



// Method to output the subpaving
// all nodes, with indentation of children
std::ostream & PCFV::outputRootToStreamTabs(
													std::ostream & os,
													int prec) const
{
	if (hasSubPaving()) {
		
		getSubPaving()->nodesAllOutput(os, 1, prec); // the output
		
	}
	
    return os;
}

// make a .dot file for the histogram
void PCFV::outputGraphDot() const
{
    bool success = false;
	if (hasSubPaving()) {
        success = getSubPaving()->outputGraphDot();

    }
    else {
        std::cerr << "Sorry, you can't make a graph without a root paving"
                << std::endl;
    }
	if (!success) {
		std::cerr << "Failed to make dot graph" << std::endl;
	}
}

// Method to add current state of this during splitting to a log file
// Output goes to file named according to argument s
// Output is textToTabs
void PCFV::outputLog(const std::string& s, 
											int i, int prec) const
{
    // To add output of the PCFV object to file
    ofstream os(s.c_str(), ios::app);         // append
    if (os.is_open()) {
		
		os << std::endl;
        os << "Pass " << i << std::endl; // numbering
        getSubPaving()->leavesOutputTabs(os, prec); // the output
		
		os.close();
    }
    else {
        std::cerr << "Error: could not open file named "
            << s << std::endl << std::endl;
    }
}



std::string PCFV::stringSummary() const 
{
	std::ostringstream oss;
	
	oss << "This address = " << (this) << endl;
	oss << "This label = " << label << endl;
	if (hasSubPaving()) oss << "Address of subpaving is " << getSubPaving() << endl;
	else oss << "Subpaving is NULL" << endl;
	
	return oss.str();
}


void PCFV::swap(PCFV& pcf) // throw()
{
	std::swap(rootPaving, pcf.rootPaving); // just swap the paving pointers
	std::swap(label, pcf.label); // and labels
}


// --------------------------- private ---------------------------------------


RealMappedSPnode* PCFV::getSubPaving() const
{return rootPaving;}


// Method to put opening line into a log file
void PCFV::outputLogStart(const std::string& s) const
{
    // Make a string with filename and timestamp to start log file
    time_t rawtime;
    struct tm * timeinfo;
    time ( &rawtime );
    timeinfo = localtime ( &rawtime );
    ofstream os(s.c_str());         // replace data
    if (os.is_open()) {
        os << "File " << s << " created " <<  asctime (timeinfo) << std::endl;
        os.close();
    }
    else {
        std::cerr << "Error: could not open file named "
            << s << std::endl << std::endl;
    }
}


// normalise this
void PCFV::_normalise()
{
	getSubPaving()->normalise();
}

//marginalise this
void PCFV::_marginalise(
						const std::vector<int>& reqDims)
{
	getSubPaving()->marginalise(reqDims);
}

// internal method to find coverage
cxsc::real PCFV::_coverage(const rvector& pt) const
{
	
	real cov = 0.0;
	
	real totalArea = getTotalIntegral();
	
	//if total integral is 0, coverage will always be 0
	if (totalArea > cxsc::real(0.0) ) {
	
		const RealMappedSPnode * container = 
						getSubPaving()->findContainingNode(pt);

		if (container != NULL) {
			
			real culmArea = totalArea;
		
			// put the leaves into a vector and sort it, smallest to largest
			// put the leaves into a vector
			RealMappedSPnode::ConstPtrs leaves;
			getSubPaving()->getConstLeaves(leaves);
			
			sort(leaves.begin(), leaves.end(), nodePtrCompare);
		
			bool found = false;
			cxsc::real containerRange = container -> getRange();
			
			RealMappedSPnode::ConstPtrs::const_reverse_iterator
										rit = leaves.rbegin();
			
			while (!found && rit < leaves.rend()) {
				
				// check the boxes
				// stop at the first box not taller than this one
				cxsc::real thisRange = (*rit)->getRange();
				
				if ( thisRange > containerRange ) {
					// decrement cumulative area
					culmArea -= (*rit)->getRealAreaRangeWithBox();
				}
				else { 
					found = true;	// break out of loop
				}
				++rit;
				
			} // end while
				
			// if we have not found we have a problem,
			// since findContainingNode said it was here somewhere
		
			if (!found) {
				throw std::logic_error(
				"PCFV::_coverage(const rvector&) : lost container");
			}	
			
			cov = culmArea/totalArea;
		}
	}
	return cov;
}

//find nodes constituting density region with cov coverage
// checks that 0 <= cov <= 1;
RealMappedSPnode::ConstPtrs&
	PCFV::findCoverageRegion(
	RealMappedSPnode::ConstPtrs& covNodes,
	cxsc::real cov) const
{
	if (!hasSubPaving()) {
		throw NullSubpavingPointer_Error(
					"PCFV::findCoverageRegion(...)");
	}
	if (cov < 0.0 || cov > 1.0) {
		throw std::invalid_argument("PCFV::findCoverageRegion(...) : cov");
	}
	if (getSubPaving()->hasNegativeRangeInTree()) {
		throw std::runtime_error(
			"PCFV::findCoverageRegion(...) : subpaving has a negative range");
	}
	if (getSubPaving()->hasInfiniteRangeInTree()) {
		throw std::runtime_error(
			"PCFV::findCoverageRegion(...) : subpaving has an infinite range");
	}
	
	RealMappedSPnode::ConstPtrs tmp;
	
	if (cov > 0.0) {
		// put the leaves into a vector
		RealMappedSPnode::ConstPtrs leaves;
		getSubPaving()->getConstLeaves(leaves);
		
		sort(leaves.begin(), leaves.end(), nodePtrCompare);
		
		/* if cov = 1 we'd just want all the leaves but don't
		 * bother to optimise with if etc for this */
		
		//start iterating from the largest
		RealMappedSPnode::ConstPtrs::const_reverse_iterator rit = leaves.rbegin();
		bool found = false; //found the boxes that gives cov density region
	
		real totalArea = getTotalIntegral();
		// the total area of this might not be 1
		cxsc::dotprecision unnormCov(0.0);
		cxsc::accumulate(unnormCov, totalArea, cov);
		
		#ifdef DEBUGCOVERAGEREGION
			cout << "totalArea is " << totalArea << endl;
			cout << "unnormed coverage value is " << rnd(unnormCov) << endl;
		#endif
		
		cxsc::dotprecision totalCov(0.0);
		
		while (!found && rit < leaves.rend()) {
			
			//accumulate the range * box vol
			accumulate(totalCov, (*rit)->nodeRealVolume(), (*rit)->getRange()); 

			#ifdef DEBUGCOVERAGEREGION
				cout << "node is " << ((*rit)->getNodeName()) << endl;
				cout << "totalCov is now " << rnd(totalCov) << endl;
			#endif
		

			//push back the node that fulfill the condition unnormCov >= totalCov
			//into the container tmp
			if (!(unnormCov < totalCov)) { 
				#ifdef DEBUGCOVERAGEREGION
					cout << "adding " << ((*rit)->getNodeName()) << endl;
				#endif
				tmp.push_back((*rit)); 
			} 
		
			// found is true if totalCov >= unnormCov
			#ifdef DEBUGCOVERAGEREGION
				cout << "!(totalCov < unnormCov) = " << !(totalCov < unnormCov) << endl;
			#endif
			found = !(totalCov < unnormCov) ;
			
			++rit;				
		} // end while
		
		assert(found);
		
		
	}	
	// tmp empty if cov <= 0.0;	
	tmp.swap(covNodes);
	
	#ifdef DEBUGCOVERAGEREGION
		if (covNodes.empty()) std::cout << "coverage region is empty" << std::endl;
		else {
			std::cout << "nodes in coverage region are: " << std::endl;
			for (RealMappedSPnode::ConstPtrsItr it = covNodes.begin();
					it < covNodes.end();
					++it) {
				std::cout << (*it)->getNodeName() << "\t";
			}
			std::cout << std::endl;
		}
	#endif
		
	return covNodes; 
} 


//check that the box is okay
bool PCFV::checkBox(const cxsc::ivector& box)
{
	// main problem is thin intervals
	int i = Lb(box);
	bool retValue = (Ub(box) - i + 1 > 0);
	
	while ( retValue && i <= Ub(box) )
	{
		retValue = ( Inf(box[i]) != Sup(box[i]) );
		++i;
	}
	return retValue;
}

void PCFV::handleSPError(
											RealMappedSPnode& spn)
{
	// restore our spn to the supplied copy
	std::swap(*(getSubPaving()), spn);
	
	std::cerr << std::endl;
			std::cerr << "Operation aborted:original subpaving restored."
			<< std::endl;
}

// ensure rootPaving is deleted if constructed in failed constructor
void PCFV::constructor_error_handler() 
{
	try {
		
			delete rootPaving;
			rootPaving = NULL;
	}
	catch (std::exception const& ee) {} // catch and swallow
	
	throw; // rethrow the original exception
}


// ----------------------------- non member functions

//Output all boxes in PCFV pcf
std::ostream & subpavings::operator<<(std::ostream &os, 
				const subpavings::PCFV& pcf)
{
    pcf.outputRootToStreamTabs(os);
    return os;
}


// Full specializations of the templates in std namespace can be added in std namespace.
template <>
void std::swap (subpavings::PCFV & f1, 
		subpavings::PCFV & f2) // throw ()
{
	f1.swap(f2);
}






