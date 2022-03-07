server: main.cpp ./threadpool/threadpool.h ./http/http_conn.cpp ./http/http_conn.h ./lock/locker.h ./timer/time_heap.h ./timer/time_heap.cpp
	g++ -o server main.cpp ./threadpool/threadpool.h ./http/http_conn.cpp ./http/http_conn.h ./lock/locker.h ./timer/time_heap.h ./timer/time_heap.cpp -pthread

clean:
	rm  server