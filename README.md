mkdir z-plus && cd z-plus
wget https://github.com/CrowCpp/Crow/releases/download/v1.0/crow_all.h -O crow.h
# Kwafi main.cpp da ke sama
# Compile
g++ main.cpp -o server -lpthread -lsqlite3 -std=c++17
./server

# A wani terminal
# Bude index.html a Chrome
