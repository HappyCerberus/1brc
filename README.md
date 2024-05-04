# Optimizing C++ code to run 87x faster: The One Billion Row Challenge

This is a companion repository for the [Daily bit(e) of C++ #474: Optimizing code to run 87x faster](https://open.substack.com/pub/simontoth/p/daily-bite-of-c-optimizing-code-to?r=1g4l8a&utm_campaign=post&utm_medium=web).

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

If you want to also test against a random input, you can use the `create_measurements3.sh` instead.
