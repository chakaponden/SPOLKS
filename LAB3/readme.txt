MOD:
parallel TCP server + parallel UDP server at the same time

need ncurces library for client async input (OOB send works with tcp use only)

[install ncurses lib]
cd ncurses-5.9
./configure
sudo make install 

[compile]
gcc <srcFile> -lncurses