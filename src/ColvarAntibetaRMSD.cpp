#include "MultiColvarSecondaryStructureRMSD.h"
#include "ActionRegister.h"
#include "PlumedMain.h"
#include "Atoms.h"

namespace PLMD {

//+PLUMEDOC MCOLVAR ANTIBETARMSD
/**

\par Examples

*/
//+ENDPLUMEDOC

class ColvarAntibetaRMSD : public MultiColvarSecondaryStructureRMSD {
public:
  static void registerKeywords( Keywords& keys );
  ColvarAntibetaRMSD(const ActionOptions&);
}; 

PLUMED_REGISTER_ACTION(ColvarAntibetaRMSD,"ANTIBETARMSD")

void ColvarAntibetaRMSD::registerKeywords( Keywords& keys ){
  MultiColvarSecondaryStructureRMSD::registerKeywords( keys );
  keys.add("compulsory","STYLE","all","Antiparallel beta sheets can either form in a single chain or from a pair of chains. If STYLE=all all "
                                      "chain configuration with the appropriate geometry are counted.  If STYLE=inter "
                                      "only sheet-like configurations involving two chains are counted, while if STYLE=intra "
                                      "only sheet-like configurations involving a single chain are counted");
}

ColvarAntibetaRMSD::ColvarAntibetaRMSD(const ActionOptions&ao):
Action(ao),
MultiColvarSecondaryStructureRMSD(ao)
{
  // read in the backbone atoms
  std::vector<std::string> backnames(5); std::vector<unsigned> chains;
  backnames[0]="N"; backnames[1]="CA"; backnames[2]="CB"; backnames[3]="C"; backnames[4]="O";
  readBackboneAtoms( backnames, chains );

  bool intra_chain,inter_chain; 
  std::string style; parse("STYLE",style);
  if( style=="all" ){ 
      intra_chain=true; inter_chain=true;
  } else if( style=="inter"){
      intra_chain=false; inter_chain=true;
  } else if( style=="intra"){
      intra_chain=true; inter_chain=false;
  } else {
      error( style + " is not a valid directive for the STYLE keyword");
  }

  // This constructs all conceivable sections of antibeta sheet in the backbone of the chains
  if( intra_chain ){
    unsigned nres, nprevious=0; std::vector<unsigned> nlist(30);
    for(unsigned i=0;i<chains.size();++i){
       if( chains[i]<40 ) error("segment of backbone is not long enough to form an antiparallel beta hairpin. Each backbone fragment must contain a minimum of 8 residues");
       // Loop over all possible triples in each 8 residue segment of protein
       nres=chains[i]/5; plumed_assert( chains[i]%5==0 );
       for(unsigned ires=0;ires<nres-7;ires++){
           for(unsigned jres=ires+7;jres<nres;jres++){
               for(unsigned k=0;k<15;++k){
                  nlist[k]=nprevious + ires*5+k;
                  nlist[k+15]=nprevious + (jres-2)*5+k;
               }
               addColvar( nlist );
           }
       }
       nprevious+=chains[i];
    }
  }
  // This constructs all conceivable sections of antibeta sheet that form between chains
  if( inter_chain ){
      if( chains.size()==1 && style!="all" ) error("there is only one chain defined so cannot use inter_chain option");
      unsigned iprev,jprev,inres,jnres; std::vector<unsigned> nlist(30);
      for(unsigned ichain=1;ichain<chains.size();++ichain){
         iprev=0; for(unsigned i=0;i<ichain;++i) iprev+=chains[i];
         inres=chains[ichain]/5; plumed_assert( chains[ichain]%5==0 ); 
         for(unsigned ires=0;ires<inres-2;++ires){
            for(unsigned jchain=0;jchain<ichain;++jchain){
                jprev=0; for(unsigned i=0;i<jchain;++i) jprev+=chains[i];
                jnres=chains[jchain]/5; plumed_assert( chains[jchain]%5==0 );
                for(unsigned jres=0;jres<jnres-2;++jres){
                    for(unsigned k=0;k<15;++k){
                       nlist[k]=iprev+ ires*5+k;
                       nlist[k+15]=jprev+ jres*5+k;
                    } 
                    addColvar( nlist );
                }
            }
         }
      } 
  }

  // Build the reference structure ( in angstroms )
  std::vector<Vector> reference(30);
  reference[0]=Vector( 2.263, -3.795,  1.722); // N    i
  reference[1]=Vector( 2.493, -2.426,  2.263); // CA
  reference[2]=Vector( 3.847, -1.838,  1.761); // CB
  reference[3]=Vector( 1.301, -1.517,  1.921); // C
  reference[4]=Vector( 0.852, -1.504,  0.739); // O
  reference[5]=Vector( 0.818, -0.738,  2.917); // N    i+1
  reference[6]=Vector(-0.299,  0.243,  2.748); // CA
  reference[7]=Vector(-1.421, -0.076,  3.757); // CB
  reference[8]=Vector( 0.273,  1.680,  2.854); // C
  reference[9]=Vector( 0.902,  1.993,  3.888); // O
  reference[10]=Vector( 0.119,  2.532,  1.813); // N    i+2
  reference[11]=Vector( 0.683,  3.916,  1.680); // CA
  reference[12]=Vector( 1.580,  3.940,  0.395); // CB
  reference[13]=Vector(-0.394,  5.011,  1.630); // C
  reference[14]=Vector(-1.459,  4.814,  0.982); // O
  reference[15]=Vector(-2.962,  3.559, -1.359); // N    j-2
  reference[16]=Vector(-2.439,  2.526, -2.287); // CA
  reference[17]=Vector(-1.189,  3.006, -3.087); // CB
  reference[18]=Vector(-2.081,  1.231, -1.520); // C
  reference[19]=Vector(-1.524,  1.324, -0.409); // O
  reference[20]=Vector(-2.326,  0.037, -2.095); // N    j-1
  reference[21]=Vector(-1.858, -1.269, -1.554); // CA
  reference[22]=Vector(-3.053, -2.199, -1.291); // CB
  reference[23]=Vector(-0.869, -1.949, -2.512); // C
  reference[24]=Vector(-1.255, -2.070, -3.710); // O
  reference[25]=Vector( 0.326, -2.363, -2.072); // N    j
  reference[26]=Vector( 1.405, -2.992, -2.872); // CA
  reference[27]=Vector( 2.699, -2.129, -2.917); // CB
  reference[28]=Vector( 1.745, -4.399, -2.330); // C
  reference[29]=Vector( 1.899, -4.545, -1.102); // O

  // Store the secondary structure ( last number makes sure we convert to internal units nm )
  setSecondaryStructure( reference, 0.17/atoms.getUnits().length, 0.1/atoms.getUnits().length ); 
}

}
