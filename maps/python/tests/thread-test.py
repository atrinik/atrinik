def main2():
    import threading
    from Common import find_obj

    sack = find_obj(activator, name = "sack")

    def timer(objs):
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

        t = threading.Timer(0.005, timer, [objs])
        t.start()

    t = threading.Timer(0.005, timer, [list()])
    t.start()

def main2():
    import threading

    def timer():
        pl.DrawInfo("hi 1!")
        pl.DrawInfo("hi 2!")
        pl.DrawInfo("hi 3!")
        pl.DrawInfo("hi 4!")
        t = threading.Timer(0.005, timer)
        t.start()

    t = threading.Timer(0.005, timer)
    t.start()