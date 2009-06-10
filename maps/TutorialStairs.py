import Atrinik
import string

me = Atrinik.WhoAmI()
ac = Atrinik.WhoIsActivator()
map1 = Atrinik.ReadyMap("/stoneglow/stoneglow_0000",0)

ac.SetSaveBed(map1, 22, 7)
