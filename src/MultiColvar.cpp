#include "MultiColvar.h"
#include "PlumedMain.h"
#include <vector>
#include <string>

using namespace std;
using namespace PLMD;

void MultiColvar::registerKeywords( Keywords& keys ){
  Action::registerKeywords( keys );
  ActionWithValue::registerKeywords( keys );
  ActionAtomistic::registerKeywords( keys );
  keys.addFlag("PBC",true,"use the periodic boundary conditions when calculating distances");
  keys.addFlag("NOPBC",false,"ignore the periodic boundary conditions when calculating distances");
  keys.reserve("optional","NL_CUTOFF","the cutoff for the neighbour list");
  keys.reserve("optional","NL_STRIDE","the frequency with which the neighbour list should be updated");
  keys.reserve("hidden","NL_CHEAT","a cheat keyword to tell us if we are using products");
  keys.reserve("atoms","ATOMS","the atoms involved in each of the collective variables you wish to calculate. "
                               "To compute a single CV use ATOMS.  If you use ATOMS1, ATOMS2, ATOMS3... multiple CVs "
                               "will be calculated - one for each ATOM keyword you specify (all ATOM keywords should "
                               "define the same number of atoms).  The eventual number of quantities calculated by this "
                               "action will depend on what functions of the distribution you choose to calculate."); 
  keys.reserve("atoms","GROUP","this keyword is used for colvars that are calculated from a pair of atoms. "
                               "One colvar is calculated for each distinct pair of atoms in the group.");
  keys.reserve("atoms","GROUPA","this keyword is used for colvars that are calculate from a pair of atoms and must appaer with the keyword GROUPB. "
                                "Every pair of atoms which involves one atom from GROUPA and one atom from GROUPB defines one colvar");
  keys.reserve("atoms","GROUPB","this keyword is used for colvars that are calculate from a pair of atoms and must appaer with the keyword GROUPA. "
                                "Every pair of atoms which involves one atom from GROUPA and one atom from GROUPB defines one colvar");
  keys.reserve("atoms","SPECIES","this keyword is used for colvars such as coordination number. In that context it specifies that plumed should calculate "
                                 "one coordination number for each of the atoms specified.  Each of these coordination numbers specifies how many of the "
                                 "other specified atoms are within a certain cutoff of the central atom.");
  keys.reserve("atoms","SPECIESA","this keyword is used for colvars such as the coordination number.  In that context it species that plumed should calculate "
                                  "one coordination number for each of the atoms specified in SPECIESA.  Each of these cooordination numbers specifies how many "
                                  "of the atoms specifies using SPECIESB is within the specified cutoff");
  keys.reserve("atoms","SPECIESB","this keyword is used for colvars such as the coordination number.  It must appear with SPECIESA.  For a full explanation see " 
                                  "the documentation for that keyword");
  ActionWithDistribution::registerKeywords( keys );
} 

void MultiColvar::useNeighbourList( const std::string& style, Keywords& keys ){
  keys.use("NL_CUTOFF"); keys.use("NL_STRIDE");
  if(style=="product") keys.use("NL_CHEAT");
}

MultiColvar::MultiColvar(const ActionOptions&ao):
Action(ao),
ActionAtomistic(ao),
ActionWithValue(ao),
ActionWithDistribution(ao),
readatoms(false),
usepbc(true),
setupList(true),
updateFreq(0),
lastUpdate(0),
reduceAtNextStep(false)
{
  if( keywords.style("NOPBC", "flag") ){ 
    bool nopbc=!usepbc; parseFlag("NOPBC",nopbc);
    usepbc=!nopbc; parseFlag("PBC",usepbc);
  }

  // Setup the neighbour list
  if( keywords.exists("NL_CUTOFF") ){
      double rcut=0; parse("NL_CUTOFF",rcut);
      if( rcut>0 ){
          if( keywords.exists("NL_CHEAT") ) nlist.setup("product",rcut);
          else nlist.setup("sum",rcut); 
          parse("NL_STRIDE",updateFreq);
          if( updateFreq==0 ) error("found neighbour list cutoff but no frequency for update use NL_STRIDE");
      } 
      setupList=false; 
  }
}

void MultiColvar::readAtoms( int& natoms ){
  if( keywords.exists("ATOMS") ) readAtomsKeyword( natoms );
  if( keywords.exists("GROUP") ) readGroupsKeyword( natoms );
  if( keywords.exists("SPECIES") ) readSpeciesKeyword( natoms );

  if( !readatoms ) error("No atoms have been read in");

  // Now set up all the lists for the first time
  for(unsigned i=0;i<colvar_atoms.size();++i){
     colvar_atoms[i].activateAll();
     colvar_atoms[i].updateActiveMembers();
     activateLinks( colvar_atoms[i], all_atoms );
  }
  all_atoms.updateActiveMembers();

  readDistributionKeywords();  // And read the ActionWithDistributionKeywords
  requestAtoms();              // Request the atoms in ActionAtomistic and set up the value sizes
}

void MultiColvar::createNeighbourList( std::vector<std::pair<unsigned,unsigned> >& pairs ){
  if( !keywords.exists("NL_CUTOFF") ) error("You have not asserted that you are using the neighbour list when registering keywords");
  
  if( updateFreq>0 ){
     for(unsigned i=0;i<pairs.size();++i) nlist.addPair( pairs[i].first, pairs[i].second ); 
     log.printf("  neighbour list cutoff is %f.  Neighbour list will be updated every %d steps\n", nlist.get_cutoff(), updateFreq );
  }
  setupList=true;
}

void MultiColvar::readAtomsKeyword( int& natoms ){ 
  if( readatoms) return; 

  std::vector<AtomNumber> t;
  parseAtomList("ATOMS",t); 
  if( t.size()!=0 ){
     readatoms=true;
     if( natoms>0 && t.size()!=natoms ){
        std::string nat; Tools::convert(natoms, nat );
        error("ATOMS keyword did not specify " + nat  + " atoms.");
     } else {
        natoms=t.size();
     }
     DynamicList newlist;
     for(unsigned i=0;i<natoms;++i){ 
        newlist.addIndexToList(i);
        all_atoms.addIndexToList( t[i].index() ); 
     }
     colvar_atoms.push_back( newlist );
     log.printf("  Colvar 1 is calculated from atoms : ");
     for(unsigned i=0;i<t.size();++i) log.printf("%d ",t[i].serial() );
     log.printf("\n");
  } else {
     bool readone=false; DynamicList newlist;
     for(int i=1;;++i ){
        parseAtomList("ATOMS", i, t );
        if( t.size()==0 ) break;

        log.printf("  Colvar %d is calculated from atoms : ", i);
        for(unsigned i=0;i<t.size();++i) log.printf("%d ",t[i].serial() );
        log.printf("\n"); 

        if( i==1 && natoms<0 ) natoms=t.size();
        if( t.size()!=natoms ){
            std::string ss; Tools::convert(i,ss); 
            error("ATOMS" + ss + " keyword has the wrong number of atoms"); 
        }
        for(unsigned j=0;j<natoms;++j){ 
           newlist.addIndexToList( natoms*(i-1)+j ); 
           all_atoms.addIndexToList( t[j].index() );
        }
        t.resize(0); colvar_atoms.push_back( newlist );
        newlist.clear(); readatoms=true;
     }
  }
}

void MultiColvar::readGroupsKeyword( int& natoms ){
  if( readatoms ) return;

  if( natoms==2 ){
      if( !keywords.exists("GROUPA") ) error("use GROUPA and GROUPB keywords as well as GROUP");
      if( !keywords.exists("GROUPB") ) error("use GROUPA and GROUPB keywords as well as GROUP");
  } else {
      error("Cannot use groups keyword unless the number of atoms equals 2");
  }
  
  std::vector<AtomNumber> t;
  parseAtomList("GROUP",t);
  if( t.size()!=0 ){
      readatoms=true;
      for(unsigned i=0;i<t.size();++i) all_atoms.addIndexToList( t[i].index() );
      DynamicList newlist; 
      for(unsigned i=1;i<t.size();++i){ 
          for(unsigned j=0;j<i;++j){ 
             newlist.addIndexToList(i); newlist.addIndexToList(j);
             colvar_atoms.push_back( newlist ); newlist.clear();
             log.printf("  Colvar %d is calculated from atoms : %d %d \n", colvar_atoms.size(), t[i].serial(), t[j].serial() ); 
          }
      }
  } else {
      std::vector<AtomNumber> t1,t2; 
      parseAtomList("GROUPA",t1);
      if( t1.size()!=0 ){
         readatoms=true;
         parseAtomList("GROUPB",t2);
         if ( t2.size()==0 ) error("GROUPB keyword defines no atoms or is missing. Use either GROUPA and GROUPB or just GROUP"); 
         for(unsigned i=0;i<t1.size();++i) all_atoms.addIndexToList( t1[i].index() ); 
         for(unsigned i=0;i<t2.size();++i) all_atoms.addIndexToList( t2[i].index() ); 
         DynamicList newlist;
         for(unsigned i=0;i<t1.size();++i){
             for(unsigned j=0;j<t2.size();++j){
                 newlist.addIndexToList(i); newlist.addIndexToList( t1.size() + j );
                 colvar_atoms.push_back( newlist ); newlist.clear();
                 log.printf("  Colvar %d is calculated from atoms : %d %d \n", colvar_atoms.size(), t1[i].serial(), t2[j].serial() );
             }
         }
      }
  }
}

void MultiColvar::readSpeciesKeyword( int& natoms ){
  if( readatoms ) return ;

  if( !keywords.exists("SPECIESA") ) error("use SPECIESA and SPECIESB keywords as well as SPECIES");
  if( !keywords.exists("SPECIESB") ) error("use SPECIESA and SPECIESB keywords as well as SPECIES");

  std::vector<AtomNumber> t;
  parseAtomList("SPECIES",t);
  if( t.size()!=0 ){
      readatoms=true; natoms=t.size();
      for(unsigned i=0;i<t.size();++i) all_atoms.addIndexToList( t[i].index() );
      DynamicList newlist;
      for(unsigned i=0;i<t.size();++i){
          newlist.addIndexToList(i);
          log.printf("  Colvar %d involves central atom %d and atoms : ", colvar_atoms.size()+1,t[i].serial() );
          for(unsigned j=0;j<t.size();++j){
              if(i!=j){ newlist.addIndexToList(j); log.printf("%d ",t[j].serial() ); }
          }
          log.printf("\n");
          colvar_atoms.push_back( newlist ); newlist.clear();
      }
  } else {
      std::vector<AtomNumber> t1,t2;
      parseAtomList("SPECIESA",t1);
      if( t1.size()!=0 ){
         readatoms=true; 
         parseAtomList("SPECIESB",t2);
         if ( t2.size()==0 ) error("SPECIESB keyword defines no atoms or is missing. Use either SPECIESA and SPECIESB or just SPECIES");
         natoms=1+t2.size();
         for(unsigned i=0;i<t1.size();++i) all_atoms.addIndexToList( t1[i].index() );
         for(unsigned i=0;i<t2.size();++i) all_atoms.addIndexToList( t2[i].index() );
         DynamicList newlist;
         for(unsigned i=0;i<t1.size();++i){
            newlist.addIndexToList(i);
            log.printf("  Colvar %d involves central atom %d and atoms : ", colvar_atoms.size()+1,t1[i].serial() );
            for(unsigned j=0;j<t2.size();++j){
                newlist.addIndexToList( t1.size() + j ); log.printf("%d ",t2[j].serial() ); 
            }
            log.printf("\n");
            colvar_atoms.push_back( newlist ); newlist.clear(); 
         }
      }
  }
}

void MultiColvar::prepare(){
// Before a neighbour list update make sure we have the full set of atoms
  if(reduceAtNextStep){
     for(unsigned i=0;i<colvar_atoms.size();++i) activateLinks( colvar_atoms[i], all_atoms );
     all_atoms.updateActiveMembers(); requestAtoms();
     reduceAtNextStep=false;
  }
  if( updateFreq>0 && (getStep()-lastUpdate)>=updateFreq ){
     for(unsigned i=0;i<colvar_atoms.size();++i){
       colvar_atoms[i].activateAll(); colvar_atoms[i].updateActiveMembers();
     }
     all_atoms.updateActiveMembers(); requestAtoms();
     // And reset the ActionWithDistribution 
     resetMembers();
     reduceAtNextStep=true;
     lastUpdate=getStep();
  }
}

void MultiColvar::requestAtoms(){
   unsigned natoms=all_atoms.getNumberActive();
   std::vector<AtomNumber> a(natoms); 
   for(unsigned i=0;i<natoms;++i) a[i].setIndex( all_atoms[i] );

   ActionAtomistic::requestAtoms(a);
   if( usingDistributionFunctions() ){
       for(unsigned i=0;i<getNumberOfComponents();++i) getPntrToComponent(i)->resizeDerivatives(3*a.size()+9);
   } else {
       for(unsigned i=0;i<getNumberOfComponents();++i) getPntrToComponent(i)->resizeDerivatives( getThisFunctionsNumberOfDerivatives(i) );
   }
}

void MultiColvar::retrieveAtoms( const unsigned& j, std::vector<Vector>& pos ){
  unsigned natoms=colvar_atoms[j].getNumberActive();
  plumed_assert( pos.size()==natoms );
  for(unsigned i=0;i<natoms;++i){
      pos[i]=getPosition( colvar_atoms[j][i] );
  }
}

void MultiColvar::calculateThisFunction( const unsigned& j, Value* value_in ){
  plumed_massert(setupList,"You have not written any code to setup the neighbour list");
  unsigned natoms=colvar_atoms[j].getNumberActive();

  if ( natoms==0 ) return;   // Do nothing if there are no active atoms in the colvar
  Tensor vir; std::vector<Vector> pos(natoms), der(natoms); 
  // Retrieve the atoms
  retrieveAtoms( j, pos );

  // Update the neighbour list if it is time
  if( reduceAtNextStep ){
      // Update the neighbour list
      nlist.update( pos, usepbc, getPbc(), colvar_atoms[j] );
      // Get the new number of atoms
      natoms=colvar_atoms[j].getNumberActive();
      // Return if there are no atoms 
      if( natoms==0 ) return;
      // Resize everything if there are atoms
      pos.resize( natoms ); der.resize( natoms );
      // Resize the value's derivatives
      value_in->resizeDerivatives( 3*natoms + 9 );
      // Re-retrieve the atoms
      retrieveAtoms( j, pos ); 
  }
  
  // Compute the derivatives
  double value=compute( pos, der, vir );
  // Put all this in the value we are passing back
  for(unsigned i=0;i<natoms;++i){
      value_in->addDerivative( 3*i+0,der[i][0] );
      value_in->addDerivative( 3*i+1,der[i][1] );
      value_in->addDerivative( 3*i+2,der[i][2] );
  }
  value_in->addDerivative( 3*natoms+0, vir(0,0) );
  value_in->addDerivative( 3*natoms+1, vir(0,1) );
  value_in->addDerivative( 3*natoms+2, vir(0,2) );
  value_in->addDerivative( 3*natoms+3, vir(1,0) );
  value_in->addDerivative( 3*natoms+4, vir(1,1) );
  value_in->addDerivative( 3*natoms+5, vir(1,2) );
  value_in->addDerivative( 3*natoms+6, vir(2,0) );
  value_in->addDerivative( 3*natoms+7, vir(2,1) );
  value_in->addDerivative( 3*natoms+8, vir(2,2) );

  // And store the value
  value_in->set(value);
}

void MultiColvar::mergeDerivatives( const unsigned j, Value* value_in, Value* value_out ){    

  int thisatom; unsigned innat=colvar_atoms[j].getNumberActive();
  for(unsigned i=0;i<innat;++i){
     thisatom=linkIndex( i, colvar_atoms[j], all_atoms );
     plumed_assert( thisatom>=0 ); 
     value_out->addDerivative( 3*thisatom+0, value_in->getDerivative(3*i+0) );
     value_out->addDerivative( 3*thisatom+1, value_in->getDerivative(3*i+1) );
     value_out->addDerivative( 3*thisatom+2, value_in->getDerivative(3*i+2) ); 
  }

  // Easy to merge the virial
  unsigned outnat=getNumberOfAtoms(); 
  value_out->addDerivative( 3*outnat+0, value_in->getDerivative(3*innat+0) );
  value_out->addDerivative( 3*outnat+1, value_in->getDerivative(3*innat+1) );
  value_out->addDerivative( 3*outnat+2, value_in->getDerivative(3*innat+2) );
  value_out->addDerivative( 3*outnat+3, value_in->getDerivative(3*innat+3) );
  value_out->addDerivative( 3*outnat+4, value_in->getDerivative(3*innat+4) );
  value_out->addDerivative( 3*outnat+5, value_in->getDerivative(3*innat+5) );
  value_out->addDerivative( 3*outnat+6, value_in->getDerivative(3*innat+6) );
  value_out->addDerivative( 3*outnat+7, value_in->getDerivative(3*innat+7) );
  value_out->addDerivative( 3*outnat+8, value_in->getDerivative(3*innat+8) );
}

Vector MultiColvar::getSeparation( const Vector& vec1, const Vector& vec2 ) const {
  if(usepbc){ return pbcDistance( vec1, vec2 ); }
  else{ return delta( vec1, vec2 ); }
}

void MultiColvar::apply(){
  vector<Vector>&   f(modifyForces());
  Tensor&           v(modifyVirial());

  for(unsigned i=0;i<f.size();i++){
    f[i][0]=0.0;
    f[i][1]=0.0;
    f[i][2]=0.0;
  }
  v.clear();

  unsigned nat=getNumberOfAtoms(); std::vector<double> forces; unsigned nder;
  if( usingDistributionFunctions() ) forces.resize(3*getNumberOfAtoms()+9);

  for(int i=0;i<getNumberOfComponents();++i){
     nder=getThisFunctionsNumberOfDerivatives(i);
     if( !usingDistributionFunctions() && forces.size()!=nder ) forces.resize(nder);
 
     if( getPntrToComponent(i)->applyForce( forces ) ){
        for(unsigned j=0;j<nat;++j){
           f[j][0]+=forces[3*j+0];
           f[j][1]+=forces[3*j+1];
           f[j][2]+=forces[3*j+2];
        }
        v(0,0)+=forces[3*nat+0];
        v(0,1)+=forces[3*nat+1];
        v(0,2)+=forces[3*nat+2];
        v(1,0)+=forces[3*nat+3];
        v(1,1)+=forces[3*nat+4];
        v(1,2)+=forces[3*nat+5];
        v(2,0)+=forces[3*nat+6];
        v(2,1)+=forces[3*nat+7];
        v(2,2)+=forces[3*nat+8];
     }
  }
}





