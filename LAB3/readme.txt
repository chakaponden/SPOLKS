MOD:
parallel TCP server + parallel UDP server at the same time

look, there are problems if udp and tcp client send file with
the same filename - there is no sync with tcp and udp fileNames from clients
because tcp and udp servers have different client lists - clientVect[MAX_PENDING]

need ncurces library for client async input (OOB send works with tcp use only)

[install ncurses lib]
cd ncurses-5.9
./configure
sudo make install 

[compile]
gcc <srcFile> -lncurses