OUT_DIR=`readlink -m "docker-output"`
mkdir -p $OUT_DIR
rm -rf $OUT_DIR/*
docker run --privileged -v $OUT_DIR:/mv3c/test/scripts/output/ -it mv3c .
sudo chown -R $USER $OUT_DIR/*
