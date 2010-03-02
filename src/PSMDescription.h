/*******************************************************************************
 Copyright 2006-2009 Lukas Käll <lukas.kall@cbr.su.se>

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.

 *******************************************************************************/
#ifndef PSMDESCRIPTION_H_
#define PSMDESCRIPTION_H_
#include <set>
#include <map>
#include <vector>
#include <string>
using namespace std;
#include "Enzyme.h"

class PSMDescription {
  public:
    PSMDescription();
    virtual ~PSMDescription();
    void clear() {
      proteinIds.clear();
    }
    double * getFeatures() {
      return features;
    }
    double * getRetentionFeatures() {
      return retentionFeatures;
    }
    string& getPeptide() {
      return peptide;
    }
    string& getFullPeptide() {
      return getAParent()->peptide;
    }
    PSMDescription* getAParent() {
      if (parentFragment)
        return parentFragment->getAParent();
      else
        return this;
    }
    double getUnnormalizedRetentionTime() {
      return unnormalize(retentionTime);
    }
    static bool isSubPeptide(string& child, string& parent);
    bool isNotEnzymatic() {
      return !(Enzyme::isEnzymatic(peptide[0], peptide[2])
          && Enzyme::isEnzymatic(peptide[peptide.size() - 3],
                                 peptide[peptide.size() - 1])
          && Enzyme::countEnzymatic(peptide) == 0);
    }
    void
    checkFragmentPeptides(vector<PSMDescription>::reverse_iterator other,
                          vector<PSMDescription>::reverse_iterator theEnd);
    static void setRetentionTime(vector<PSMDescription>& psms, map<int,
        double>& scan2rt);
    static double unnormalize(double normalizedTime);

    static double normDiv, normSub;

    double q, pep;
    double * features;
    double * retentionFeatures;
    double retentionTime, predictedTime, massDiff, pI;
    unsigned int scan;
    string id;
    string peptide;
    set<string> proteinIds;
    PSMDescription *parentFragment;
};

inline bool const operator<(PSMDescription const& one,
                            PSMDescription const& other) {
  if (one.peptide == other.peptide) return one.retentionTime
      < other.retentionTime;
  return one.peptide < other.peptide;
}

inline bool operator==(PSMDescription const& one,
                       PSMDescription const& other) {
  //	return one.peptide == other.peptide;
  if (one.peptide == other.peptide)
    return true;
  else
    return false;
}

/*
 inline bool operator!=(const PSMDescription& one, const PSMDescription& other){
 return one.peptide != other.peptide;
 }
 */

#endif /*PSMDESCRIPTION_H_*/
