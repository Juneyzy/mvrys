X=PR_AGENT-$2

version=`lsb_release -i -s`
version_id=`lsb_release -r -s`
machine=`uname -m`
issue_date=`date "+%Y%m%d"`

tarball="HY-$X-$2-$version$version_id-$machine-$issue_date".tar.gz


do_mkdir(){
    mkdir release
    rm $X -rf
    mkdir $X
    mkdir $X/bin
    mkdir $X/script
}

do_copy(){
    #copy binary to bin dir
    mv $1 $X/bin
    cp ../conf/ $X/ -rf
    cp script/ls_metafiles.sh $X/script/
}

do_tarball(){
    tar -zcvf $1 $X
    #move tarball to release dirent
    mv $1 release/
    rm $X -rf
    echo "`date`  BUILD=$X  COMMIT=$3 ARCHIVE=$tarball" >> release/release.log
}

do_mkdir
do_copy $1
do_tarball $tarball
