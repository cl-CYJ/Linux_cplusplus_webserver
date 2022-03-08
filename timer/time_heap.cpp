#include <utility>
#include <iostream>
#include "/home/cyj/linux/Linux_cplusplus_webserver/timer/time_heap.h"
 
time_heap::time_heap(int cap)
    :capacity(cap), cur_size(0) {
    //创建堆数组
    array = new heap_timer*[capacity];
    if (!array) {
        fprintf(stderr, "init heap_timer failed.\n");
        return;
    }
 
    for (int i = 0; i < capacity; i ++) {
        array[i] = NULL;
    }
}
 
//构造之二：用数组来初始化堆
time_heap::time_heap(heap_timer **init_array, int size, int capacity)
    :capacity(capacity), cur_size(size) {
    if (capacity < size) {
        fprintf(stderr, "init heap_timer 1 for init_array failed.\n");
        return;
    }

    //创建堆数组
    array = new heap_timer*[capacity];
    if (!array) {
        fprintf(stderr, "init heap_timer 2 failed.\n");
        return;
    }
 
    for (int i = 0; i < capacity; i ++) {
        array[i] = NULL;
    }

    if (size != 0) {
        //初始化堆数组
        for (int i = 0; i < size; i ++) {
            array[i] = init_array[i];
            array[i]->position = i;
        }
        //对数组中第 (cur_size-1)/2 ~ 0 个元素执行下潜操作
        for (int i = (cur_size-1)/2; i >=0; i --) {
            percolate_down(i);
        }
    }
}
 
time_heap::~time_heap() {
    for (int i = 0; i < cur_size; i ++) {
        delete array[i];
    }
    delete[] array;
}
 
int time_heap::add_timer(heap_timer *timer) {
   if (!timer) return -1;
 
   //如果堆数组不够大，将其扩大1倍
   if (cur_size >= capacity)
       resize();
 
   //新插入一个元素，当前堆大小加1, element是新建空穴的位置
   int element = cur_size++;
   int parent = 0;
 
   //对从空穴到根节点的路径上的所有节点执行上虑操作
   for ( ; element > 0; element = parent) {
       parent = (element -1) /2;

       if (array[parent]->expire <= timer->expire) break;
 
       array[element] = array[parent];
       array[element]->position = element;
   }
 
   array[element] = timer;
   array[element]->position = element;
   #ifdef DEBUG
   
       std::cout << "加入时cb_func为: " << array[element]->cb_func << std::endl;
   
   #endif 
 
   return 0;
}

//调整更新了expire的定时器在堆中的位置
void time_heap::adjust_node(heap_timer* node) {
    percolate_down(node->position);
}
 
void time_heap::del_timer(heap_timer *timer) {
    if (!timer) return;

    //将定时器移至堆顶销毁，防止数组在高并发持续访问下膨胀崩溃
    int element = timer->position;
    int parent = 0;
    timer->expire = -20;
    for ( ; element > 0; element = parent) {
       parent = (element -1) / 2;
       array[element] = array[parent];
       array[element]->position = element;
    }
    array[element] = timer;
    array[element]->position = element;
    if (array[0] == timer) pop_timer();
    else throw std::exception();
}
 
heap_timer* time_heap::top() const {
    if (empty())
        return NULL;
 
    return array[0];
}
 
//删除堆顶元素，同时生成新的堆顶定时器
void time_heap::pop_timer() {
    if (empty()) {
        return;
    }
 
    if (array[0]) {
        delete array[0];
 
        //如果还有元素，将原来的堆顶元素替换为堆数组中最后一个元素
        if (--cur_size > 0) {
            array[0] = array[cur_size];
            array[0]->position = 0;
 
            //对新的堆顶元素执行下潜操作
            percolate_down(0);
        } 
    }
}
 
void time_heap::tick() {
    heap_timer *tmp = array[0];
    time_t cur = time(NULL);
 
    //循环处理到期定时器
    while (!empty()) {
        if (!tmp) break;

        #ifdef DEBUG
        
            printf("tmp->expire - cur = %ld\n", (tmp->expire) - cur);
        
        #endif 
        //如果堆顶定时期没到期，则退出循环
        if (tmp->expire > cur) {
            break;
        }

        #ifdef DEBUG
        
            printf("array[0]->position: %d\n", array[0]->position);
            printf("array[0]->cb_func: %d\n", array[0]->cb_func);            
        
        #endif 

        //否则就执行堆顶定时器中的回调函数     
        if (array[0]->cb_func) {
            #ifdef DEBUG
        
                printf("执行了回调函数: ");
        
            #endif 
            array[0]->cb_func(array[0]->user_data);
        }
 
        //删除堆元素，同时生成新的堆顶定时器
        pop_timer();
        
        tmp = array[0];
    }
    printf("堆中还有%d个元素\n", cur_size);
}

//将一个点下潜
void time_heap::percolate_down(int element) {
    int tmp = element;
    int lchild = (element * 2) + 1;
    int rchild = (element * 2) + 2;

    if (lchild <= (cur_size - 1) && array[lchild]->expire < array[element]->expire) tmp = element * 2 + 1;
    if (rchild <= (cur_size - 1) && array[rchild]->expire < array[element]->expire) tmp = element * 2 + 2;
    if (tmp != element)
    {
        heap_swap(element, tmp);
        percolate_down(tmp);
    }
}

//交换堆中的两个heap_timer
void time_heap::heap_swap(int pos1, int pos2) {
    std::swap(array[pos1], array[pos2]);
    std::swap(array[pos1]->position, array[pos2]->position);
}
 
//对堆进行扩容，每次扩容两倍，注意删除原来分配的内存
void time_heap::resize() {
    heap_timer **tmp = new heap_timer*[2 * capacity];
 
    for (int i = 0; i < (capacity * 2); i ++) {
        tmp[i] = NULL;
    }

    if (!tmp) {
        fprintf(stderr, "resize() failed.\n");
        return;
    }
 
    capacity = 2 * capacity;
 
    for (int i = 0; i < cur_size; i ++) {
        tmp[i] = array[i];
        tmp[i]->position = i;
    }
    delete[] array;
    array = tmp;
}
 
 