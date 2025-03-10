Hálózat nyilvántartó, monitorozó és vagyonvédelmi szoftver Qt/C++ alapokon.

A program alapja egy adatbázis, ami leképezi a teljes hálózatot. Erre épül egy lekérdező rendszer, melynek kettős feladata van. Egyrészt adatot gyüjt, feltöltve az adatbázist a lekérdezhető elemekkel, másrészt támogatja a manuális adatbevitelt. Ezen fellül a lekérdező rendszer hasonló feladatokat is elláthat, mint a Nagios rendszer. Végül a rendelkezésre álló állapot adatok lehetővé teszik, hogy vagyonvédelmi funkciókat is ellásson. A rendszer ki lett egészítve hardwer elemekkel, a nem aktív hálózati elemek védelmére.

A program azoknak lehet hasznos, akik nagyobb kliens ill. végpont számú hálózatot menedzselnek, és nem csak a szerverek működésére kiváncsiak.

A dokumentáció folyamatosan készül, akár naponta változhat.

A rendszer dokumentációja a project fájlok között: LanView2.odt

Egy rövid leírás, és egy prezentáció: lv2pre.odt lanview.pptx

Az interpreter dokumentációja : import/import_man.odt 
Az API dokumentáció pedig : http://svn.kkfk.bgf.hu/lanview2.doc/LanView2_API/ címen.
Az adatbázis dokumentáció : http://svn.kkfk.bgf.hu/lanview2.doc/database/ cimen.
(Sajnos az postgresql_autodoc csak egy bekezdést kezel egy megjegyzésnél, nálam meg több bekezdés is van, ezért ezek osszefolynak.)

A rendszer elemei:

lv2	Program könyvtár: a lanview2 rendszer API (a GUI nélkül)

lv2g	Program könyvtár: a lanview2 rendszer API a GUI-hoz.

lv2gui	A lanview2 alapértelmezett GUI-ja.

lv2d	A lanview2 rendszer szerver keret programja.

import	A rendszer interpretere, ill. interpreter távoli végrehajtás szerver pr.

portstat (pstat) Switch portok állapot lekérdező pr.

portvlan Switch portok VLAN kiosztásának a lekérdezése.

portmac Switch portok címtábláit lekérdező pr.

arpd	Az ARP táblákat, vagy DHCP konfig állományokat lekérdező pr.

snmpvars Álltalános SNMP értékek lekérdezése.

rrdhelper RRD fájlok kezelése

icontsrv A vagyonvédelmi kiegészítő hardever elemeket lekérdező program.

updt_oui Az OUI rekordokat frissítő program.

database Mappa: az adatbázis

design	Mappa: a vagyonvédelmi hardvare elemek gyártási dokumentációja

firmware Mappa: a vagyonvédelmi hw elemek fimware

Megjegyzés: Az SNMPlekérdezések viszonylag kevés álltalában HP gyártmányű switch-eken lettek tesztelve.
Sajnos a gyártok az ajánlásokat elég lazán kezelik, így nincs garancia arra, hogy más gyártótol való
eszközökön is jól működnek a lkérdezések. Ez különösen igaz az LLDP felfedezésre.

Megjegyzés: Jelenleg én aktívan használom a programot, és a felmerülő igények szerint bővítem is. Sajnos a fejlesztő 'csapat'
létszéma (1 fő) nincs igazán öszhangban a feladat méretével. Ezért idő hiányában, továbbá a visszajelzések teljes hiányának
következtében viszonylag kevés figyelmet kap a modosítások problémamentes lekövetése, a dokumentációk frissítése.
Ezért, ha valaki használni kívánja a programot, az legyen szíves jelezzen!
A program honosítható, ehhez minden elemet tartalmaz, de a honosításhoz nekem sem időm, sem kellő nyelvtudásom mincsen.


