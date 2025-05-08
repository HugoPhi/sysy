- Sys-Y Code: 

```c
int a = 0;
int arr[2] = {2, 4};
int func(int p){
    p = p - 1;
    return p;
}
int main(){
    int b;
    a = 10;
    b = func(a);
    return b;
}
```

- IR code: 

```text
void global()
    0: def a, 0
    1: alloc arr, 2
    2: store 2, arr, 1
    3: store 4, arr, 2
    4: return null

int func(int p)
    0: subi t0, p, 1     # p = p - 1
    1: mov p, t0         # 更新p的值
    2: return p          # 返回p

int main()
    0: call t0, global() # 初始化全局变量
    1: def a, 10         # a = 10
    2: call t1, func(a)  # 调用func(a)
    3: mov b, t1         # b = 返回值
    4: return b          # 返回b

GVT:
    a int 0              # 全局变量a（初始值0）
    arr int_ptr 2        # 全局数组arr，长度2
```