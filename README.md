Readme for reproducibility submission of paper 
Transaction Repair for Multi-Version Concurrency Control (http://dl.acm.org/citation.cfm?id=3035919)

# A) Source code info
- Repository: [https://github.com/epfldata/mv3c](https://github.com/epfldata/mv3c)
- Programming Language: mostly C++
- Additional Programming Language info: C++ 11
- Compiler Info:  gcc version 5.4.0 20160609 (Ubuntu 5.4.0-6ubuntu1~16.04.4)
- Packages/Libraries Needed: Docker

# B) Datasets info
Same as source code. (Combined)


# C) Hardware Info

### C1) Processor (architecture, type, and number of processors/sockets)
Dual-socket Intel® Xeon® CPU E5-2680  2.50GHz (12 physical cores) with 256 GB RAM
Our preparation script disables hyperthreading and the second socket


### C2) Caches (number of levels, and size of each level)
L1d cache:             32K
L1i cache:             32K
L2 cache:              256K
L3 cache:              30720K

### C3) Memory (size and speed)
256 GB DDR4 - 2133 MHz

### C4) Secondary Storage (type: SSD/HDD/other, size, performance: random read/sequnetial read/random write/sequnetial write)
HDD 180 GB. Specifications don't matter as our experiments are in-memory.

### C5) Network (if applicable: type and bandwidth)
N/A


# D) Experimentation Info

### D1) Scripts and how-tos to generate all necessary data or locate datasets

Experiments "6" and "7" generate data and commands internally, whereas the other experiments get their data from ThirdParty open-source projects that are included in this reproducibility package. A snapshot of the data is part of the package, however, the GENDATA variable can be set (in MV3C_MultiThreaded/tests/scripts/all.sh) to generate fresh data.

### D2) Scripts and how-tos to prepare the software for system

All experiments are packaged inside a Docker container. As a prerequisite, you need to install Docker. Here is the detailed instructions for installing Docker on your platform: https://docs.docker.com/engine/installation.

For ease of use, simple "docker-build.sh" and "docker-run.sh" scripts are provided which build and run the Docker container respectively. Docker containers are run in privileged mode so that preparation scripts turn off hyperthreading and correctly pin each thread to one core.

The root scripts directory is /app/MV3C_MultiThreaded/tests/scripts (within the Docker container). Scripts for executing (multi-threaded) experiments are in the "execute" folder. Similarly, scripts for processing the data (collecting from multiple experiments, aggregation etc.) are in the "process" folder, and finally "plot" contains all the gnuplot scripts. The "all.sh" script in the root scripts directory calls all the subscripts after setting the necessary environment variables. You can selectively comment out one or more experiments in all.sh to exclude them.

In summary, pLease follow these steps:
- Step 1: add your user to the "sudo" and "docker" groups.
  In Ubuntu, you can run "sudo usermod -aG sudo YOUR_USER_NAME && sudo usermod -aG docker YOUR_USER_NAME".

- Step 2: execute "./docker-build.sh" script once to build the Docker image.

- Step 3: execute "./docker-run.sh" to run the previously built Docker image and run the experiments.

### D3) Scripts and how-tos for all experiments executed for the paper

After executing "docker-run.sh", you automatically enter the docker instance and the current directory should be "/app/MV3C_MultiThreaded/tests/scripts". Please follow this last step to run the experiments:

- Step 4: execute "./all.sh" to run all the experiments.

The outputs are generated in the "output" directory in the root scripts directory /app/MV3C_MultiThreaded/tests/scripts (within the Docker container). This directory is mapped to the "output" directory in the host machine. The "output" directory contains four directories:

- executable: contains the compiled binaries for each experiment
- raw: contains the raw output for each experiment
- average: contains the processed and aggregated version of "raw" data
- graphs: contains the output PDF files for each figure using the "average" data