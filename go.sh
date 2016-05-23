
WORKERS=${1:-1}
PORT=${2:-80}

gcc -Wall http_server.c -o http_server -pthread && ./http_server $WORKERS $PORT
rm -f http_server

