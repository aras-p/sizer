// Executable size report utility.
// Aras Pranckevicius, http://aras-p.info/projSizer.html
// Based on code by Fabian "ryg" Giesen, http://farbrausch.com/~fg/
// Public domain.

#include "pdbfile.hpp"
#include <cstdio>
#include <ctime>

int main( int argc, char** argv )
{
  if( argc < 2 ) {
    fprintf( stderr, "Usage: Sizer <exefile>\n" );
    return 1;
  }

  DebugInfo info;

  clock_t time1 = clock();

  info.Init();
  PDBFileReader pdb;
  fprintf( stderr, "Reading debug info file %s ...\n", argv[1] );
  bool pdbok = pdb.ReadDebugInfo( argv[1], info );
  if( !pdbok ) {
    fprintf( stderr, "ERROR reading file via PDB\n" );
    return 1;
  }
  fprintf( stderr, "\nProcessing info...\n" );
  info.FinishedReading();
  info.StartAnalyze();
  info.FinishAnalyze();

  fprintf( stderr, "Generating report...\n" );
  std::string report = info.WriteReport();

  clock_t time2 = clock();
  float secs = float(time2-time1) / CLOCKS_PER_SEC;

  fprintf( stderr, "Printing...\n" );
  puts( report.c_str() );
  fprintf( stderr, "Done in %.2f seconds!\n", secs );


  return 0;
}
