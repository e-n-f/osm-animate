osm-animate
-----------

Based on [osmlab/osm-animate](https://github.com/osmlab/osm-animate).

Uses [datamaps](https://github.com/ericfischer/datamaps), pngquant, gifsicle, and netpbm. If on a Mac,

    brew install pngquant gifsicle netpbm libpng

If on Linux,

    sudo apt-get install gifsicle pngquant libpng-dev libexpat1-dev

And in either case:

    git clone https://github.com/ericfischer/datamaps.git
    cd datamaps
    make

Then come back here and make "snap":

    cd ..
    make snap

You need an OSM extract with timestamps, like the ones that [Mapzen hosts](https://mapzen.com/data/metro-extracts). For instance, let's try Brasilia. Copy the URL for the OSM XML download for the city you want, and download it:

    curl -O https://s3.amazonaws.com/metro-extracts.mapzen.com/brasilia_brazil.osm.bz2

Then extract the vectors from the OSM data and encode them with datamaps:

    bzcat brasilia_brazil.osm.bz2 | ./snap > brasilia.snap
    cat brasilia.snap | ./datamaps/encode -z20 -m32 -o brasilia.dm

And then do:

    ./perform brasilia

which reads brasilia.snap to find the bounds of the area and the range of times represented,
and makes multiple renderings of brasilia.dm into brasilia.out and ultimately combines them
into brasilia.gif.
