#include <string.h>

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#include "GPU/GPUEngine.h"
#include "Kangaroo.h"
#include "SECPK1/SECP256k1.h"
#include "Timer.h"

using namespace std;

#define CHECKARG(opt, n)                        \
  if (a >= argc - 1) {                          \
    ::printf(opt " missing argument #%d\n", n); \
    exit(0);                                    \
  } else {                                      \
    a++;                                        \
  }

// ------------------------------------------------------------------------------------------
void printUsage() {
    const char *usage =
        "Options:\n"
        " -v                     Print version\n"
        " -gpu                   Enable GPU calculation\n"
        " -gpuId gpuId1,gpuId2   List of GPU(s) to use, default is 0\n"
        " -g g1x,g1y,g2x,g2y     Specify GPU(s) kernel grid size, default is 2*(MP), 2*(Core/MP)\n"
        " -d dpBit               Specify number of leading zeros for the DP method (default is auto)\n"
        " -t nbThread            Specify number of threads\n"
        " -w workfile            Specify file to save work into (current processed key only)\n"
        " -i workfile            Specify file to load work from (current processed key only)\n"
        " -wi workInterval       Periodic interval (in seconds) for saving work\n"
        " -ws                    Save kangaroos in the work file\n"
        " -wss                   Save kangaroos via the server\n"
        " -wsplit                Split work file of server and reset hashtable\n"
        " -wm file1 file2 destfile  Merge work files\n"
        " -wmdir dir destfile    Merge directory of work files\n"
        " -wt timeout            Save work timeout in milliseconds (default is 3000ms)\n"
        " -winfo file1           Work file info file\n"
        " -wpartcreate name      Create empty partitioned work file (name is a directory)\n"
        " -wcheck workfile       Check workfile integrity\n"
        " -m maxStep             Number of operations before giving up the search (maxStep * expected operations)\n"
        " -s                     Start in server mode\n"
        " -c server_ip           Start in client mode and connect to server at server_ip\n"
        " -sp port               Server port, default is 17403\n"
        " -nt timeout            Network timeout in milliseconds (default is 3000ms)\n"
        " -o fileName            Output result to fileName\n"
        " -l                     List CUDA enabled devices\n"
        " -check                 Check GPU kernel vs CPU\n"
        " inFile                 Input configuration file\n";

    printf("%s", usage);
    exit(0);
}

// ------------------------------------------------------------------------------------------

int getInt(string name, char *v) {
  int r;

  try {
    r = std::stoi(string(v));

  } catch (std::invalid_argument &) {
    printf("Invalid %s argument, number expected\n", name.c_str());
    exit(-1);
  }

  return r;
}

double getDouble(string name, char *v) {
  double r;

  try {
    r = std::stod(string(v));

  } catch (std::invalid_argument &) {
    printf("Invalid %s argument, number expected\n", name.c_str());
    exit(-1);
  }

  return r;
}

// ------------------------------------------------------------------------------------------

void getInts(string name, vector<int> &tokens, const string &text, char sep) {
  size_t start = 0, end = 0;
  tokens.clear();
  int item;

  try {
    while ((end = text.find(sep, start)) != string::npos) {
      item = std::stoi(text.substr(start, end - start));
      tokens.push_back(item);
      start = end + 1;
    }

    item = std::stoi(text.substr(start));
    tokens.push_back(item);

  } catch (std::invalid_argument &) {
    printf("Invalid %s argument, number expected\n", name.c_str());
    exit(-1);
  }
}
// ------------------------------------------------------------------------------------------

// Default params
static int dp = -1;
static int nbCPUThread;
static string configFile = "";
static bool checkFlag = false;
static bool gpuEnable = false;
static vector<int> gpuId = {0};
static vector<int> gridSize;
static string workFile = "";
static string checkWorkFile = "";
static string iWorkFile = "";
static uint32_t savePeriod = 60;
static bool saveKangaroo = false;
static bool saveKangarooByServer = false;
static string merge1 = "";
static string merge2 = "";
static string mergeDest = "";
static string mergeDir = "";
static string infoFile = "";
static double maxStep = 0.0;
static int wtimeout = 3000;
static int ntimeout = 3000;
static int port = 17403;
static bool serverMode = false;
static string serverIP = "";
static string outputFile = "found.txt";
static bool splitWorkFile = false;

int main(int argc, char *argv[]) {
  time_t currentTime = time(nullptr);
  printf("\033[01;33m[+] Kangaroo v" RELEASE " [256 range edition]\n");
  std::cout << "\r\033[01;33m[+] " << ctime(&currentTime);
  // Global Init
  Timer::Init();
  rseed(Timer::getSeed32());

  // Init SecpK1
  Secp256K1 *secp = new Secp256K1();
  secp->Init();

  int a = 1;
  nbCPUThread = Timer::getCoreNumber();

  while (a < argc) {
    if (strcmp(argv[a], "-t") == 0) {
      CHECKARG("-t", 1);
      nbCPUThread = getInt("nbCPUThread", argv[a]);
      a++;
    } else if (strcmp(argv[a], "-d") == 0) {
      CHECKARG("-d", 1);
      dp = getInt("dpSize", argv[a]);
      a++;
    } else if (strcmp(argv[a], "-h") == 0) {
      printUsage();
    } else if (strcmp(argv[a], "-l") == 0) {
#ifdef WITHGPU
      GPUEngine::PrintCudaInfo();
#else
      printf("GPU code not compiled, use -DWITHGPU when compiling.\n");
#endif
      exit(0);

    } else if (strcmp(argv[a], "-w") == 0) {
      CHECKARG("-w", 1);
      workFile = string(argv[a]);
      a++;
    } else if (strcmp(argv[a], "-i") == 0) {
      CHECKARG("-i", 1);
      iWorkFile = string(argv[a]);
      a++;
    } else if (strcmp(argv[a], "-wm") == 0) {
      CHECKARG("-wm", 1);
      merge1 = string(argv[a]);
      CHECKARG("-wm", 2);
      merge2 = string(argv[a]);
      a++;
      if (a < argc) {
        // classic merge
        mergeDest = string(argv[a]);
        a++;
      }
    } else if (strcmp(argv[a], "-wmdir") == 0) {
      CHECKARG("-wmdir", 1);
      mergeDir = string(argv[a]);
      CHECKARG("-wmdir", 2);
      mergeDest = string(argv[a]);
      a++;
    } else if (strcmp(argv[a], "-wcheck") == 0) {
      CHECKARG("-wcheck", 1);
      checkWorkFile = string(argv[a]);
      a++;
    } else if (strcmp(argv[a], "-winfo") == 0) {
      CHECKARG("-winfo", 1);
      infoFile = string(argv[a]);
      a++;
    } else if (strcmp(argv[a], "-o") == 0) {
      CHECKARG("-o", 1);
      outputFile = string(argv[a]);
      a++;
    } else if (strcmp(argv[a], "-wi") == 0) {
      CHECKARG("-wi", 1);
      savePeriod = getInt("savePeriod", argv[a]);
      a++;
    } else if (strcmp(argv[a], "-wt") == 0) {
      CHECKARG("-wt", 1);
      wtimeout = getInt("timeout", argv[a]);
      a++;
    } else if (strcmp(argv[a], "-nt") == 0) {
      CHECKARG("-nt", 1);
      ntimeout = getInt("timeout", argv[a]);
      a++;
    } else if (strcmp(argv[a], "-m") == 0) {
      CHECKARG("-m", 1);
      maxStep = getDouble("maxStep", argv[a]);
      a++;
    } else if (strcmp(argv[a], "-ws") == 0) {
      a++;
      saveKangaroo = true;
    } else if (strcmp(argv[a], "-wss") == 0) {
      a++;
      saveKangarooByServer = true;
    } else if (strcmp(argv[a], "-wsplit") == 0) {
      a++;
      splitWorkFile = true;
    } else if (strcmp(argv[a], "-wpartcreate") == 0) {
      CHECKARG("-wpartcreate", 1);
      workFile = string(argv[a]);
      Kangaroo::CreateEmptyPartWork(workFile);
      exit(0);
    } else if (strcmp(argv[a], "-s") == 0) {
      if (serverIP != "") {
        printf("-s and -c are incompatible\n");
        exit(-1);
      }
      a++;
      serverMode = true;
    } else if (strcmp(argv[a], "-c") == 0) {
      CHECKARG("-c", 1);
      if (serverMode) {
        printf("-s and -c are incompatible\n");
        exit(-1);
      }
      serverIP = string(argv[a]);
      a++;
    } else if (strcmp(argv[a], "-sp") == 0) {
      CHECKARG("-sp", 1);
      port = getInt("serverPort", argv[a]);
      a++;
    } else if (strcmp(argv[a], "-gpu") == 0) {
      gpuEnable = true;
      a++;
    } else if (strcmp(argv[a], "-gpuId") == 0) {
      CHECKARG("-gpuId", 1);
      getInts("gpuId", gpuId, string(argv[a]), ',');
      a++;
    } else if (strcmp(argv[a], "-g") == 0) {
      CHECKARG("-g", 1);
      getInts("gridSize", gridSize, string(argv[a]), ',');
      a++;
    } else if (strcmp(argv[a], "-v") == 0) {
      ::exit(0);
    } else if (strcmp(argv[a], "-check") == 0) {
      checkFlag = true;
      a++;
    } else if (a == argc - 1) {
      configFile = string(argv[a]);
      a++;
    } else {
      printf("Unexpected %s argument\n", argv[a]);
      exit(-1);
    }
  }

  if (gridSize.size() == 0) {
    for (int i = 0; i < gpuId.size(); i++) {
      gridSize.push_back(0);
      gridSize.push_back(0);
    }
  } else if (gridSize.size() != gpuId.size() * 2) {
    printf("Invalid gridSize or gpuId argument, must have coherent size\n");
    exit(-1);
  }

  Kangaroo *v =
      new Kangaroo(secp, dp, gpuEnable, workFile, iWorkFile, savePeriod,
                   saveKangaroo, saveKangarooByServer, maxStep, wtimeout, port,
                   ntimeout, serverIP, outputFile, splitWorkFile);
  if (checkFlag) {
    v->Check(gpuId, gridSize);
    exit(0);
  } else {
    if (checkWorkFile.length() > 0) {
      v->CheckWorkFile(nbCPUThread, checkWorkFile);
      exit(0);
    }
    if (infoFile.length() > 0) {
      v->WorkInfo(infoFile);
      exit(0);
    } else if (mergeDir.length() > 0) {
      v->MergeDir(mergeDir, mergeDest);
      exit(0);
    } else if (merge1.length() > 0) {
      v->MergeWork(merge1, merge2, mergeDest);
      exit(0);
    }
    if (iWorkFile.length() > 0) {
      if (!v->LoadWork(iWorkFile)) exit(-1);
    } else if (configFile.length() > 0) {
      if (!v->ParseConfigFile(configFile)) exit(-1);
    } else {
      if (serverIP.length() == 0) {
        ::printf("No input file to process\n");
        exit(-1);
      }
    }
    if (serverMode)
      v->RunServer();
    else
      v->Run(nbCPUThread, gpuId, gridSize);
  }

  return 0;
}