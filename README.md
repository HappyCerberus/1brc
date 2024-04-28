# 1brc
The One Billion Row Challenge using C++


## Generating input

If you want to generate input for the 1brc challenge please follow the documentation at [gunnarmorling/1brc](https://github.com/gunnarmorling/1brc?tab=readme-ov-file#running-the-challenge).

The `gunnarmorling/1brc` repository is part of this repository as a submodule.

```
git submodule init
pushd 1brc
./create_measurements.sh 1000000000
./calculate_average_baseline.sh >golden_output.txt
popd
mv 1brc/measurements.txt .
mv 1brc/golden_output.txt .
```