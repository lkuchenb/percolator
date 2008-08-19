/*******************************************************************************
 Copyright (c) 2008 Lukas K�ll

 Permission is hereby granted, free of charge, to any person
 obtaining a copy of this software and associated documentation
 files (the "Software"), to deal in the Software without
 restriction, including without limitation the rights to use,
 copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the
 Software is furnished to do so, subject to the following
 conditions:

 The above copyright notice and this permission notice shall be
 included in all copies or substantial portions of the Software. 
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 OTHER DEALINGS IN THE SOFTWARE.
 
 $Id: PosteriorEstimator.cpp,v 1.19 2008/08/19 10:27:14 lukall Exp $
 
 *******************************************************************************/

#include<vector>
#include<utility>
#include<cstdlib>
#include<fstream>
#include<sstream>
#include<iterator>
#include<algorithm>
#include<numeric>
#include<functional>
using namespace std;
#include "Option.h"
#include "ArrayLibrary.h"
#include "LogisticRegression.h"
#include "PosteriorEstimator.h"
#include "Transform.h"
#include "Globals.h"

static unsigned int noIntevals = 500;
static unsigned int numLambda = 100;
static double maxLambda = 0.99;

bool PosteriorEstimator::reversed = false;
bool PosteriorEstimator::pvalInput = false;

PosteriorEstimator::PosteriorEstimator()
{
}

PosteriorEstimator::~PosteriorEstimator()
{
}

pair<double,bool> make_my_pair(double d,bool b) {
  return make_pair(d,b);
}

class IsDecoy {public: bool operator() (const pair<double,bool>& aPair) 
    {return !aPair.second; }};

bool isMixed(const pair<double,bool>& aPair) {
  return aPair.second;
}

// This is the slow version of the function from revision 1.12.
template<class T> void bootstrap_old(const vector<T>& in, vector<T>& out) {
  out.clear();
  double n = in.size();
  for (size_t ix=0;ix<n;++ix) {
    size_t draw = (size_t)((double)rand()/((double)RAND_MAX+(double)1)*n);
    out.push_back(in[draw]);
  }
  // sort in desending order
  sort(out.begin(),out.end());  
} 


template<class T> void bootstrap(const vector<T>& in, vector<T>& out, size_t max_size = 1000) {
  out.clear();
  double n = in.size();
  size_t num_draw = min(in.size(),max_size);
  for (size_t ix=0;ix<num_draw;++ix) {
    size_t draw = (size_t)((double)rand()/((double)RAND_MAX+(double)1)*n);
    out.push_back(in[draw]);
  }
  // sort in desending order
  sort(out.begin(),out.end());  
} 

double mymin(double a,double b) {return a>b?b:a;}

void PosteriorEstimator::estimatePEP( vector<pair<double,bool> >& combined, double pi0, vector<double>& peps) {
  // Logistic regression on the data
  size_t nTargets=0,nDecoys=0;
  LogisticRegression lr;
  estimate(combined,lr,pi0);

  vector<double> xvals(0);
   
  vector<pair<double,bool> >::const_iterator elem = combined.begin();  
  for(;elem != combined.end();++elem)
    if (elem->second) {
      xvals.push_back(elem->first);
      ++nTargets;
    } else
      ++nDecoys;
      
  lr.predict(xvals,peps);
  
  double factor = pi0*((double)nTargets/(double)nDecoys);
  double top = min(1.0,factor*exp(*max_element(peps.begin(),peps.end())));
  vector<double>::iterator pep = peps.begin(); 
  bool crap = false; 
  for(;pep != peps.end();++pep) {
    if (crap) {
      *pep = top;
      continue;
    }
    *pep = factor*exp(*pep);
    if (*pep>=top) {
      *pep = top;
      crap=true;
    }
  }
  partial_sum(peps.rbegin(), peps.rend(), peps.rbegin(), mymin);
  
}

void PosteriorEstimator::estimate( vector<pair<double,bool> >& combined, LogisticRegression& lr, double pi0) {
  // switch sorting order
  if (!reversed)
    reverse(combined.begin(),combined.end());

  vector<double> medians;
  vector<unsigned int> negatives,sizes;
  
  binData(combined,medians,negatives,sizes);

  lr.setData(medians,negatives,sizes);
  lr.iterativeReweightedLeastSquares();

  // restore sorting order
  if (!reversed)
    reverse(combined.begin(),combined.end());
}

// Estimates q-values and prints
void PosteriorEstimator::finishStandalone(vector<pair<double,bool> >& combined, const vector<double>& peps, double pi0) { 
  vector<double> q(0),xvals(0);
   
  getQValues(pi0,combined,q);

  vector<pair<double,bool> >::const_iterator elem = combined.begin();  
  for(;elem != combined.end();++elem)
    if (elem->second)
      xvals.push_back(elem->first);
      
  vector<double>::iterator xval=xvals.begin();
  vector<double>::const_iterator qv = q.begin(),pep = peps.begin();

  cout << "Score\tPEP\tq-value" << endl;
  
  for(;xval != xvals.end();++xval,++pep,++qv)
    cout << *xval << "\t" << *pep << "\t" << *qv << endl;
}


void PosteriorEstimator::binData(const vector<pair<double,bool> >& combined,  
  vector<double>& medians, vector<unsigned int>& negatives, vector<unsigned int>& sizes) {
  // Create bins and count number of negatives in each bin
  double binSize = max(combined.size()/(double)noIntevals,1.0); 
  vector<pair<double,bool> >::const_iterator combinedIter=combined.begin();  

  unsigned int binNo =0;
  size_t firstIx, pastIx = 0; 

  while (pastIx<combined.size()) {
    firstIx = pastIx; pastIx = min(combined.size(),(size_t)((++binNo)*binSize)); 
    // Handle ties
    while ((pastIx<combined.size()) &&
          (combined[pastIx-1].first==combined[pastIx].first)) 
      ++pastIx;
    int inBin = pastIx-firstIx;
    int negInBin = count_if(combinedIter,combinedIter+inBin,IsDecoy());
    combinedIter += inBin;
    double median = combined[firstIx+inBin/2].first;
    if (medians.size()>0 and *(medians.rbegin())==median) {
      *(sizes.rbegin()) += inBin;
      *(negatives.rbegin()) += negInBin;
    } else {
       medians.push_back(median);
       sizes.push_back(inBin);
       negatives.push_back(negInBin);
    }    
  }
  if(VERB>1) cerr << "Binned data into " << medians.size() << " bins with average size of " <<  binSize << " samples" << endl;       
}



void PosteriorEstimator::getQValues(double pi0,
     const vector<pair<double,bool> >& combined, vector<double>& q) {
  // assuming combined sorted in decending order
  vector<pair<double,bool> >::const_iterator myPair = combined.begin();
  unsigned int nTargets = 0, nDecoys = 0;
  while(myPair != combined.end()) {
    if (!(myPair->second)) {
      ++nDecoys; 
    } else {
      ++nTargets;
      q.push_back(((double)nDecoys)/(double)nTargets);
    }
    ++myPair;
  }
  double factor = pi0*((double)nTargets/(double)nDecoys);
  transform(q.begin(), q.end(), q.begin(), bind2nd(multiplies<double>(), factor));
  partial_sum(q.rbegin(), q.rend(), q.rbegin(), mymin);
  return;  
}

void PosteriorEstimator::getPValues(
     const vector<pair<double,bool> >& combined, vector<double>& p) {
  // assuming combined sorted in best hit first order
  vector<pair<double,bool> >::const_iterator myPair = combined.begin();
  size_t nDecoys = 0, posSame =0, negSame =0;
  double prevScore = - 4711.4711; // number that hopefully never turn up first in sequence
  while(myPair != combined.end()) {
    if (myPair->first != prevScore) {
      for (size_t ix=0;ix<posSame;++ix)
        p.push_back((double)nDecoys+(((double)negSame)/(double)(posSame+1))*(ix+1));
      nDecoys += negSame;
      negSame=0; posSame=0;
      prevScore = myPair->first;
    }
    if (myPair->second) {
      ++posSame;
    } else {
      ++negSame; 
    }
    ++myPair;
  }
  transform(p.begin(), p.end(), p.begin(), bind2nd(divides<double>(), (double)nDecoys));
  // p sorted in acending order  
  return;  
}

/*
 * Described in Storey, "A direct approach to false discovery rates."
 * JRSS 2002.
 */
double PosteriorEstimator::estimatePi0(vector<double>& p, const unsigned int numBoot) {
  
  vector<double> pBoot,lambdas,pi0s,mse;
  vector<double>::iterator start;
    
  size_t n = p.size();
  // Calculate pi0 for different values for lambda    
  // N.B. numLambda and maxLambda are global variables.
  for(unsigned int ix=0; ix <= numLambda; ++ix) {
    double lambda = ((ix+1)/(double)numLambda)*maxLambda;
    // Find the index of the first element in p that is < lambda.
    // N.B. Assumes p is sorted in ascending order. 
    start = lower_bound(p.begin(),p.end(),lambda);
    // Calculates the difference in index between start and end
    double Wl = (double) distance(start,p.end());
    double pi0 = Wl/n/(1-lambda);

    if (pi0>0.0) {
      lambdas.push_back(lambda);
      pi0s.push_back(pi0);
    }
  }
     
  double minPi0 = *min_element(pi0s.begin(),pi0s.end()); 

  // Initialize the vector mse with zeroes.
  fill_n(back_inserter(mse),pi0s.size(),0.0); 

  // Examine which lambda level that is most stable under bootstrap   
  for (unsigned int boot = 0; boot< numBoot; ++boot) {
    // Create an array of bootstrapped p-values, and sort in ascending order.
    bootstrap<double>(p,pBoot); 

    n=pBoot.size();
    for(unsigned int ix=0; ix < lambdas.size(); ++ix) {
      start = lower_bound(pBoot.begin(),pBoot.end(),lambdas[ix]);
      double Wl = (double) distance(start,pBoot.end());
      double pi0Boot = Wl/n/(1-lambdas[ix]);
      // Estimated mean-squared error.
      mse[ix] += (pi0Boot-minPi0)*(pi0Boot-minPi0);
    }
  }   

  // Which index did the iterator get?
  unsigned int minIx = distance(mse.begin(),min_element(mse.begin(),mse.end()));
  double pi0 = max(min(pi0s[minIx],1.0),0.0); 

  if(VERB>1) cerr << "Selecting pi_0=" << pi0 << endl;
   
  return pi0;
}

void PosteriorEstimator::run() {
  ifstream target(targetFile.c_str(),ios::in),decoy(decoyFile.c_str(),ios::in);
  istream_iterator<double> tarIt(target),decIt(decoy);

  // Merge a labeled version of the two lists into a combined list
  vector<pair<double,bool> > combined;  
  vector<double> pvals;  
  if (!pvalInput) {
    transform(tarIt,istream_iterator<double>(),back_inserter(combined),
            bind2nd(ptr_fun(make_my_pair),true));
    transform(decIt,istream_iterator<double>(),back_inserter(combined),
            bind2nd(ptr_fun(make_my_pair), false)); 
  } else {
    copy(tarIt,istream_iterator<double>(),back_inserter(pvals));
    sort(pvals.begin(),pvals.end());
    transform(pvals.begin(),pvals.end(),back_inserter(combined),
            bind2nd(ptr_fun(make_my_pair),true));
  	size_t nDec = pvals.size();
  	double step = 1.0/2.0/(double)nDec;
  	for (size_t ix=0; ix<nDec; ++ix)
  	  combined.push_back(make_my_pair(step*(1+2*ix),false));
  	reversed = true;   
  }
  if(VERB>0) cerr << "Read " << combined.size() << " statistics" << endl; 
  if (reversed) {
    if(VERB>0) cerr << "Reversing all scores" << endl; 
  }

  if (reversed)
    // sorting in ascending order
    sort(combined.begin(),combined.end());
  else
  // sorting in decending order
    sort(combined.begin(),combined.end(),greater<pair<double,bool> >());  
  
  if (!pvalInput)
  	getPValues(combined,pvals);
  	 
  double pi0 = estimatePi0(pvals);
  
  vector<double> peps;
  // Logistic regression on the data
  estimatePEP(combined,pi0,peps);

  finishStandalone(combined,peps,pi0);  
}

bool PosteriorEstimator::parseOptions(int argc, char **argv){
  // init
  ostringstream intro;
  intro << "Usage:" << endl;
  intro << "   qvality [options] target_file null_file" << endl << "or" << endl;
  intro << "   qvality [options] pvalue_file" << endl << endl;
  intro << "target_file and null_file are files containing scores from a mixed model" << endl;  
  intro << "and a null model, each score separated with whitespace or line feed." << endl;
  intro << "Alternatively, accuate p-value could be provided in a single file pvalue_file." << endl;
  
  
  CommandLineParser cmd(intro.str());
  // finally parse and handle return codes (display help etc...)

  cmd.defineOption("v","verbose",
    "Set verbosity of output: 0=no processing info, 5=all, default is 2",
    "level");

  cmd.defineOption("s","epsilon-step",
    "The relative step size used as treshhold before cross validation error is calculated",
    "value");

  cmd.defineOption("n","number-of-bins",
    "The number of spline knots used when interpolating spline function. Default is 500.",
    "bins");

  cmd.defineOption("c","epsilon-cross-validation",
    "The relative crossvalidation step size used as treshhold before ending the iterations",
    "value");

  cmd.defineOption("r","reverse",
    "Indicating that the scoring mechanism is reversed i.e. that low scores are better than higher scores",
    "",TRUE_IF_SET);

  cmd.parseArgs(argc, argv);

  if (cmd.optionSet("v")) {
    Globals::getInstance()->setVerbose(cmd.getInt("v",0,10));
  }

  if (cmd.optionSet("n")) {
    noIntevals = cmd.getInt("n",1,INT_MAX);
  }

  if (cmd.optionSet("c")) {
    BaseSpline::convergeEpsilon=cmd.getDouble("c",0.0,1.0);
  }

  if (cmd.optionSet("s")) {
    BaseSpline::stepEpsilon=cmd.getDouble("s",0.0,1.0);
  }

  if (cmd.optionSet("r"))
    PosteriorEstimator::setReversed(true);

  if (cmd.arguments.size()>2) {
      cerr << "Too many arguments given" << endl;
      cmd.help();
  }
  if (cmd.arguments.size()==0) {
      cerr << "No arguments given" << endl;
      cmd.help();
  }
  targetFile = cmd.arguments[0];
  if (cmd.arguments.size()==2)
    decoyFile = cmd.arguments[1];
  else {
    PosteriorEstimator::setReversed(true);
    pvalInput = true;  
  }
  return true;
}

