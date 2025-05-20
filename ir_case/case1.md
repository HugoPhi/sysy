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
    a = arr[1];
    b = func(a);

    return b;
}
```

- IR code: 

```text
void global()
    0: def a, 0
    1: alloc arr, 2
    2: store 2, arr, 0
    3: store 4, arr, 1
    4: return null
end

int func(int p)
    0: subi t1, p, 1
    1: mov p, t1
    2: return p
end

int main()
    0: call t0, global()
    1: def b, 0
    2: load t2, arr, 1
    3: mov a, t2
    4: call t2, func(a)
    5: mov b, t2
    6: lss t3, b, a
    7: if t3 goto [pc, 2]
    8: goto [pc, 4]
    9: def t4, 2
    10: mul t5, b, t4
    11: mov b, t5
    12: return b
end

GVT:
    a int 0
    arr int* 2
```