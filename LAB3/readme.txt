need linux kernel 3.9 and above (add SO_REUSEPORT)
need ncurces library for client async input (OOB send)

[install ncurses lib]
cd ncurses-5.9
./configure
sudo make install 

[compile]
gcc <srcFile> -lncurses