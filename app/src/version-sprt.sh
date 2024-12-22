#! /bin/sh

#rm -rf versions
#mkdir versions

P=`pwd`

echo cutechess-cli \\ > $P/versions/sprt.sh
echo "-engine cmd=/home/folkert/Projects/Dog/app/src/linux-windows/build/Dorpsgek-A-to-E/dorpsgek proto=xboard name='dorpsgek' \\" >> $P/versions/sprt.sh

for i in `git tag`
do
	echo --- $i ---
	(rm -rf temp ;
	git clone --recursive --depth 1 --branch $i ssh://172.29.0.8/home/folkert/git/Dog.git temp && cd temp/app/src &&
	if [ -e linux-windows ] ; then cd linux-windows ; fi &&
      	mkdir build && cd build && CXX=g++-12 CC=gcc-12 cmake .. && make -j && mv Dog $P/versions/$i && echo "-engine cmd=./$i proto=uci arg='-t 1' name='$i' \\" >> $P/versions/sprt.sh)
done

echo "-concurrency 32 -each tc=8+0.08 book=/home/folkert/bin/data/dc-3200.bin -rounds 9000 -games 2 -bookmode disk -recover -pgnout dinges-t.pgn -site 'cutechess' -sprt elo0=0 elo1=10 alpha=0.01 beta=0.01" >> versions/sprt.sh

chmod +x $P/versions/sprt.sh
