def timer1(objs):
    for obj in objs:
        if not obj:
            try:
                objs.remove(obj)
            except:
                pass
        else:
            print(obj)

    for i in range(2):
        obj = CreateObject("coppercoin")
        objs.append(obj)

    Eval("timer1(objs)")

def timer2():
    pl.DrawInfo("hi 1!")
    pl.DrawInfo("hi 2!")
    pl.DrawInfo("hi 3!")
    pl.DrawInfo("hi 4!")
    Eval("timer2()", 0.005)

Eval("timer1(list())", 0.005)
Eval("timer2()", 0.005)
