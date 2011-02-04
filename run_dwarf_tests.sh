cd tests/dwarf > /dev/null
./run_tests.sh
if [ $? -ne 0 ]; then
  exit 1
fi
cd ../../ > /dev/null