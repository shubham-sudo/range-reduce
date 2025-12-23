#!/bin/bash
set -e

bash ./scripts/rebuild.sh

TAG=testing

ENTRY_SIZE=128
LAMBDA=0.125
ENTRIES_PER_PAGE=32
PAGES_PER_FILE=1024
SIZE_RATIO=4

INSERTS=1000000
UPDATES=1000000
POINT_QUERIES=0
POINT_DELETES=0
RANGE_QUERIES=100
SELECTIVITY=0.25
RANGE_DELETES=0
RANGE_DELETES_SEL=0

SHOW_PROGRESS=1


echo -e "\n"
echo "========================================"
echo "             Configuration              "
echo "========================================"

echo "TAG                      : $TAG"
echo "ENTRY_SIZE               : $ENTRY_SIZE"
echo "LAMBDA                   : $LAMBDA"
echo "ENTRIES_PER_PAGE         : $ENTRIES_PER_PAGE"
echo "PAGES_PER_FILE           : $PAGES_PER_FILE"
echo "SIZE_RATIO               : $SIZE_RATIO"
echo "----------------------------------------"
echo "INSERTS                  : $INSERTS"
echo "UPDATES                  : $UPDATES"
echo "POINT_QUERIES            : $POINT_QUERIES"
echo "POINT_DELETES            : $POINT_DELETES"
echo "RANGE_QUERIES            : $RANGE_QUERIES"
echo "SELECTIVITY              : $SELECTIVITY"
echo "RANGE_DELETES            : $RANGE_DELETES"
echo "RANGE_DELETES_SEL        : $RANGE_DELETES_SEL"
echo "========================================"
echo -e "\n"


EXP_DIR="experiments-${TAG}-I${INSERTS}-U${UPDATES}-Q${POINT_QUERIES}-D${POINT_DELETES}-S${RANGE_QUERIES}-Y${SELECTIVITY}-R${RANGE_DELETES}-y${RANGE_DELETES_SEL}-E${ENTRY_SIZE}-B${ENTRIES_PER_PAGE}-P${PAGES_PER_FILE}-T${SIZE_RATIO}"

mkdir -p .vstats
cd .vstats || exit
mkdir -p "$EXP_DIR"
cd "$EXP_DIR" || exit

echo "Generating specs for Tectonic..."
python3 ../../scripts/generate_specs.py \
        -I ${INSERTS} \
        -U ${UPDATES} \
        -Q ${POINT_QUERIES} \
        -D ${POINT_DELETES} \
        -S ${RANGE_QUERIES} \
        -Y ${SELECTIVITY} \
        -R ${RANGE_DELETES} \
        -y ${RANGE_DELETES_SEL} \
        -E ${ENTRY_SIZE} \
        -L ${LAMBDA}

# generating workload
echo "Generating workload file..."
../../bin/tectonic-cli generate -w workload.specs.json
echo -e ""

mkdir -p rocksdb range-reduce

# copy workload file
for target in rocksdb range-reduce; do
    cp workload.txt ./"$target"/workload.txt
done

# running workload
cd range-reduce || exit
echo "Benchmarking range-reduce..."
../../../bin/working_version \
        -E "$ENTRY_SIZE" \
        -B "$ENTRIES_PER_PAGE" \
        -P "$PAGES_PER_FILE" \
        -T "$SIZE_RATIO" \
        --progress "$SHOW_PROGRESS" > LOG.log
mv db/LOG LOG
rm -rf db
rm workload.txt
cd ..

echo -e "\n"

# running workload
cd rocksdb || exit
echo "Benchmarking rocksdb..."
../../../bin/working_version \
        -E "$ENTRY_SIZE" \
        -B "$ENTRIES_PER_PAGE" \
        -P "$PAGES_PER_FILE" \
        -T "$SIZE_RATIO" \
        --progress "$SHOW_PROGRESS" > LOG.log
mv db/LOG LOG
rm -rf db
rm workload.txt
cd ..

rm workload.txt
