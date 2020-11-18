/********************************************************************************
 *                                                                              *
 *             THERMINATOR 2: THERMal heavy-IoN generATOR 2                     *
 *                                                                              *
 * Version:                                                                     *
 *      Release, 2.0.3, 1 February 2011                                         *
 *                                                                              *
 * Authors:                                                                     *
 *      Radoslaw Ryblewski   (Radoslaw.Ryblewski@ifj.edu.pl)                    *
 *                                                                              *
 *                                                                              *
 *                                                                              *
 *                                                                              *
 * Project homepage:                                                            *
 *      http://therminator2.ifj.edu.pl/                                         *
 *                                                                              *
 * For the detailed description of the program and further references           *
 * to the description of the model please refer to                              *
 * http://arxiv.org/abs/1102.0273                                               *
 *                                                                              *
 * This code can be freely used and redistributed. However if you decide to     *
 * make modifications to the code, please, inform the authors.                  *
 * Any publication of results obtained using this code must include the         *
 * reference to arXiv:1102.0273 and the published version of it, when           *
 * available.                                                                   *
 *                                                                              *
 ********************************************************************************/

#include <sstream>
#include <TMath.h>
#include "THGlobal.h"
#include "Configurator.h"
#include "Parser.h"
#include "Model_SR.h"

using namespace TMath;
using namespace std;

extern Configurator* sMainConfig;
extern TString	sModelINI;
extern TString	sEventDIR;
extern TString	sTimeStamp;
extern int	sModel;
extern int	sRandomize;
extern int	sIntegrateSample;

Model_SR::Model_SR()
: Model(),
  mThermo(0),
  mR(0.0),  mH(0.0), mA(0.0), mT0(0.0), mGammaS(0.0)
{
}

Model_SR::Model_SR(TRandom2* aRandom)
: Model(aRandom) 
{
  mName = "SR";
  mThermo = new Thermodynamics();
  ReadParameters();
  Description();
  mHyperCube = mR * 2*TMath::Pi()*Pi() * 2*TMath::Pi()*Pi();
}

Model_SR::~Model_SR()
{
  delete mThermo;
}

double Model_SR::GetIntegrand(ParticleType* aPartType, bool finiteWidth)
{
  double dSIGMAdotP, Integrand;
  double Spin, Gs, Statistics;
  double Temp, Mu;
  double R, Phi, Theta;
  double P, PhiP, ThetaP;
  double dPdZet, Ep, kappa;
  double Mt, Pt, RapP;

// type of statistics: Bose-Einstein or Fermi-Dirac
  Spin = aPartType->GetSpin();
  Gs	= 2 * Spin + 1;
  Statistics = ( (Spin - static_cast<int>(Spin)) < 0.01 ? -1.0 : +1.0 );
  Temp	= mThermo->GetTemperature();
  Mu = mThermo->GetChemicalPotential(aPartType);
  
  // generate random spatial position
  R	    = mR * mRandom->Rndm();
  Phi	= 2.0 * Pi() * mRandom->Rndm();  
  Theta	= Pi() * mRandom->Rndm();  
    
// generate random momentum position 
  {
    double Zet = mRandom->Rndm();
    P	= Zet / (1.0 - Zet);	// 0 <= p <= Infinity
    dPdZet	= 1.0 / ( (1.0 - Zet) * (1.0 - Zet) );
  }
  PhiP	= 2.0 * Pi() * mRandom->Rndm(); 
  ThetaP	= Pi() * mRandom->Rndm();  
  
// other variables

  double spectralFunctionWeight;
  double M;
//  finiteWidth = false;
  GetParticleMass(aPartType, finiteWidth,M,spectralFunctionWeight);


  Ep	= Hypot(M,P);
  kappa = Cos(Theta) * Cos(ThetaP) + Sin(Theta) * Sin(ThetaP) * Cos(Phi - PhiP);

  Float_t tVR = TanH(mH * R);

  double Lgamma     = CosH(mH * R);
  double UdotP      = (Ep - P * tVR * kappa) * Lgamma;
  dSIGMAdotP = R*R * Sin(Theta) * (Ep - P * mA * kappa);

  // disable particle emission back to the hydro region
  if(dSIGMAdotP < 0.0) dSIGMAdotP = 0.0;

  // particle X and P coordinates - required to be initiated
  Xt = mT0 + mA * R;

  Xx = R * Cos(Phi) * Sin(Theta);
  Xy = R * Sin(Phi) * Sin(Theta);
  Xz = R * Cos(Theta);
  
  Pe = Ep;
  Px = P * Cos(PhiP) * Sin(ThetaP);
  Py = P * Sin(PhiP) * Sin(ThetaP);
  Pz = P * Cos(ThetaP);

  Pt  = Hypot(Px,Py);
  Mt  = Hypot(M,Pt);
  RapP = 1./2*TMath::Log((Pe+Pz)/(Pe-Pz));

  Float_t Ekin  = Mt * CosH(RapP);
  Float_t Px0   = Pt * Cos(PhiP);
  Float_t Py0   = Pt * Sin(PhiP);
  Float_t Pz0   = Mt * SinH(RapP);

  Px = Px0;
  Py = Py0;
  Pz = Pz0;
  Pe = Ekin;

  double fugacity 
      = Power(mThermo->GetGammaQ(),  aPartType->GetNumberQ() + aPartType->GetNumberAQ())
      * Power(mThermo->GetGammaS(),  aPartType->GetNumberS() + aPartType->GetNumberAS())
      * Exp(mThermo->GetChemicalPotential(aPartType) / Temp);
  double invFugacity = 1. / fugacity;

  int pdg = aPartType->GetPDGCode();
  //Statistics=0; // set to 0, not to have Bose-Einstein

  double T          = mThermo->GetTemperature();
  double Upsilon    = pow(mGammaS, aPartType->GetNumberS()+ aPartType->GetNumberAS()) * Exp(Mu/T);
  double DP = P * P * Sin(ThetaP) * dPdZet / Ep;
  double F  = (Gs / kTwoPi3) / (Exp(UdotP/T)/Upsilon  + Statistics);

  Integrand = F * DP *dSIGMAdotP;

  if (false) {
    cout << fugacity << "\t" << Integrand << "\t" << mThermo->GetChemicalPotential(aPartType) << "\t"
         << UdotP << "\t" << Exp(UdotP/Temp) << "\t"
         << Power(mThermo->GetGammaS(),  aPartType->GetNumberS() + aPartType->GetNumberAS()) << "\t"
         << mThermo->GetGammaS() << "\t" << aPartType->GetNumberS() << "\t" << aPartType->GetNumberAS() << endl;
  }
  return  Integrand;
  }

void Model_SR::Description()
{
  ostringstream oss;
  oss << "##################################################"<< endl;
  oss << MODEL_NAME(mName);
  oss << "# - freeze-out Cart. time  : " <<MODEL_PAR_DESC(mT0  * kHbarC,	"[fm]");
  oss << "# - radial size            : " <<MODEL_PAR_DESC(mR   * kHbarC,	"[fm]");
  oss << "# - Hubble velocity        : " <<MODEL_PAR_DESC(mH,		"[c]");
  oss << "# - hypersurface slope (A) : " <<MODEL_PAR_DESC(mA,		"[1]");
  oss << "# - gamma_S                : " <<MODEL_PAR_DESC(mGammaS,		"[1]");
  oss << "# - freeze-out temperature : " <<MODEL_PAR_DESC(mThermo->GetTemperature() * 1000.0,	"[MeV]");
  if ((mThermo->GetChemistryType() == 0) || (mThermo->GetChemistryType() == 1)) 
  {
    oss << "# - chem. potential Mu_B   : " <<MODEL_PAR_DESC(mThermo->GetMuB() * 1000.0,	"[MeV]");
    oss << "# - chem. potential Mu_I3  : " <<MODEL_PAR_DESC(mThermo->GetMuI() * 1000.0,	"[MeV]");
    oss << "# - chem. potential Mu_S   : " <<MODEL_PAR_DESC(mThermo->GetMuS() * 1000.0,	"[MeV]");
    oss << "# - chem. potential Mu_C   : " <<MODEL_PAR_DESC(mThermo->GetMuC() * 1000.0,	"[MeV]");
  }
  else
  {
    oss << "# - fugacity Lambda_I3     : " <<MODEL_PAR_DESC(mThermo->GetLambdaI(), "[1]");
    oss << "# - fugacity Lambda_Q      : " <<MODEL_PAR_DESC(mThermo->GetLambdaQ(), "[1]");
    oss << "# - fugacity Lambda_S      : " <<MODEL_PAR_DESC(mThermo->GetLambdaS(), "[1]");
    oss << "# - fugacity Lambda_C      : " <<MODEL_PAR_DESC(mThermo->GetLambdaC(), "[1]");   
    oss << "# - fugacity Gamma_Q       : " <<MODEL_PAR_DESC(mThermo->GetGammaQ(),  "[1]");
    oss << "# - fugacity Gamma_S       : " <<MODEL_PAR_DESC(mThermo->GetGammaS(),  "[1]");
    oss << "# - fugacity Gamma_C       : " <<MODEL_PAR_DESC(mThermo->GetGammaC(),  "[1]");   
  }
  oss << "# Parameters hash (CRC32)  : " <<MODEL_PAR_DESC(mHash,		"");
  oss << "# Integration samples      : " <<MODEL_PAR_DESC(sIntegrateSample,	"");
  oss << "# Random seed              : " <<MODEL_PAR_DESC((sRandomize ? "yes" : "no"),"");
  oss << "# Generation date          : " <<sTimeStamp<<" #"<<endl;
  oss << "##################################################"<< endl;
  mDescription = oss.str();
}

void Model_SR::AddParameterBranch(TTree* aTree)  
{
  Model_t_SR tPar;

  tPar.T0        = mT0  * kHbarC;
  tPar.R         = mR   * kHbarC;
  tPar.H         = mH;
  tPar.A         = mA;

  tPar.Temp      = mThermo->GetTemperature() * 1000.0;
  tPar.Chem      = mThermo->GetChemistryType();
  tPar.MuB       = mThermo->GetMuB() * 1000.0;
  tPar.MuI       = mThermo->GetMuI() * 1000.0;
  tPar.MuS       = mThermo->GetMuS() * 1000.0;
  tPar.MuC       = mThermo->GetMuC() * 1000.0; 
  
  tPar.LambdaQ       = mThermo->GetLambdaQ(); 
  tPar.LambdaI       = mThermo->GetLambdaI(); 
  tPar.LambdaS       = mThermo->GetLambdaS(); 
  tPar.LambdaC       = mThermo->GetLambdaC(); 
  
  tPar.GammaQ       = mThermo->GetGammaQ();  
  tPar.GammaS       = mThermo->GetGammaS(); 
  tPar.GammaC       = mThermo->GetGammaC(); 
 
  cout << "PRINTOUT HERE : " << _MODEL_T_FORMAT_SR_ << endl;
  aTree->Branch(_MODEL_T_BRANCH_, &tPar, _MODEL_T_FORMAT_SR_)->Fill();
}

void Model_SR::ReadParameters()
{
  Configurator*	tModelParam;
  Parser*	tParser;
  
  tModelParam = new Configurator;
  tParser     = new Parser(sModelINI.Data());
  tParser->ReadINI(tModelParam);
  delete tParser;
  
  try {
    mT0  	 = tModelParam->GetParameter("T0").Atof() / kHbarC;		// [GeV^-1]
    mR    	 = tModelParam->GetParameter("R").Atof() / kHbarC;		// [GeV^-1]
    mH		 = tModelParam->GetParameter("H").Atof();			// [1]
    mA		 = tModelParam->GetParameter("A").Atof();			// [1]
    mGammaS	 = tModelParam->GetParameter("GammaS").Atof();			// [1]

    //
    mThermo->SetTemperature(tModelParam->GetParameter("Temperature").Atof() * 0.001);	// [GeV]
    if (tModelParam->GetParameter("Chemistry") == "chemical_potential")
    {
      mThermo->SetChemistry(
				tModelParam->GetParameter("MuB").Atof() * 0.001,
			    tModelParam->GetParameter("MuI").Atof() * 0.001,
			    tModelParam->GetParameter("MuS").Atof() * 0.001,
			    tModelParam->GetParameter("MuC").Atof() * 0.001);	// [GeV]
      mThermo->SetGammas(
                            tModelParam->GetParameter("GammaQ").Atof(),
			    tModelParam->GetParameter("GammaS").Atof(),
			    tModelParam->GetParameter("GammaC").Atof()); // [1]
    }
    else if (tModelParam->GetParameter("Chemistry") == "gamma_lambda")
    {
      mThermo->SetChemistry(
				tModelParam->GetParameter("LambdaQ").Atof(),
			    tModelParam->GetParameter("LambdaI").Atof(),
			    tModelParam->GetParameter("LambdaS").Atof(),
			    tModelParam->GetParameter("LambdaC").Atof(),
                            tModelParam->GetParameter("GammaQ").Atof(),
			    tModelParam->GetParameter("GammaS").Atof(),
			    tModelParam->GetParameter("GammaC").Atof()); // [1]

      cout << "HERE PARAMETERS ARE : " << endl;
      cout << tModelParam->GetParameter("LambdaQ").Atof() << endl;
      cout << tModelParam->GetParameter("LambdaI").Atof() << endl;
      cout << tModelParam->GetParameter("LambdaS").Atof() << endl;
      cout << tModelParam->GetParameter("LambdaC").Atof() << endl;
      cout << tModelParam->GetParameter("GammaQ").Atof() << endl;
      cout << tModelParam->GetParameter("GammaS").Atof() << endl;
      cout << tModelParam->GetParameter("GammaC").Atof() << endl;
      cout << mThermo->GetGammaS() << endl;

    }
    else
    {
      PRINT_MESSAGE("\tDid not find one of the necessary model parameters.");
      exit(_ERROR_CONFIG_PARAMETER_NOT_FOUND_);
    }
  } catch (TString tError) {
    PRINT_MESSAGE("<Model_SR::ReadParameters>\tCaught exception " << tError);
    PRINT_MESSAGE("\tDid not find one of the necessary model parameters.");
    exit(_ERROR_CONFIG_PARAMETER_NOT_FOUND_);
  }
  
// calculate parameter hash 
  ostringstream oss;
  oss << sModel;
  oss << mT0;
  oss << mR;
  oss << mH;
  oss << mA;
  oss << mGammaS;
  oss << mThermo->GetTemperature() ;
  if ((mThermo->GetChemistryType() == 0) || (mThermo->GetChemistryType() == 1))
  {
    oss << mThermo->GetMuB() << mThermo->GetMuI() << mThermo->GetMuS() << mThermo->GetMuC();
  }
  else
  {
    oss << mThermo->GetLambdaQ() << mThermo->GetLambdaI() << mThermo->GetLambdaS() << mThermo->GetLambdaC();
    oss << mThermo->GetGammaQ()                           << mThermo->GetGammaS()  << mThermo->GetGammaC();
  }
  CalculateHash(TString(oss.str()));

// create event subdirectory if needed
  try {
    sEventDIR += tModelParam->GetParameter("EventSubDir");
    CreateEventSubDir();
  } catch (TString tError) {
  }
  
  delete tModelParam;
}
