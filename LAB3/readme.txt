[install ncurses lib]
cd ncurses-5.9
./configure
sudo make install 

[compile]
gcc <srcFile> -lncurses