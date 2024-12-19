#! /bin/sh

rm -rf versions
mkdir versions

echo cutechess-cli \\ > versions/sprt.sh
echo "-engine cmd=/home/folkert/Projects/Dog/app/src/linux-windows/build/Dorpsgek-A-to-E/dorpsgek proto=xboard name='dorpsgek' \\ " >> versions/sprt.sh

for i in `git tag`
do
	git checkout $i && git submodule update --force --recursive --init --remote && cmake .. && make -j && mv Dog versions/$i && echo "-engine cmd=./$i proto=uci arg='-t 1' name='$i' \\" >> versions/sprt.sh
done

echo "-concurrency 6 -each dir=/home/folkert/Projects/Dog/app/src/linux-windows/build tc=8+0.08 book=/home/folkert/bin/data/dc-3200.bin -rounds 9000 -games 2 -bookmode disk -recover -pgnout dinges.pgn -site 'cutechess' -sprt elo0=0 elo1=10 alpha=0.05 beta=0.05" >> versions/sprt.sh

chmod +x versions/sprt.sh
