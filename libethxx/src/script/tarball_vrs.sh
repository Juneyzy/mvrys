X=vrs-base-$2

version=`lsb_release -i -s`
version_id=`lsb_release -r -s`
machine=`uname -m`
issue_date=`date "+%Y%m%d"`

tarball="$X-$3-$version$version_id-$machine-$issue_date".tar.gz


do_mkdir(){
    mkdir release
    rm $X -rf
    mkdir $X
    mkdir $X/lib
    mkdir $X/include
    mkdir $X/script
}

do_copy(){
    cp example/* $X
    cp ../conf $X -rf
    cp script/ls_metafiles.sh $X/script
    cp $1 $X/lib
    cp lib/*.h $X/include
    cp fes/capture.h $X/include
    cp fes/rt_*.h $X/include
    cp components/yaml/*.h $X/include
}

do_tarball(){
    tar -zcvf $1 $X
    mv $1 release
    rm $X -rf
    echo "`date`  BUILD=$X  COMMIT=$3 ARCHIVE=$tarball" >> release/release.log
}

do_mkdir
do_copy $1
do_tarball $tarball

