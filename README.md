nofuzz-quake2
=============

No fuzz quake2.exe (zquake2.exe) for windows 
(keep quake2 oldschool features, keep legacy features, add http download etc.)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Alternative to q2pro,r1q2 - decided to make a quake2 client that is as close to the original as possible
while only adding essential features, like http download, avi output etc.
the user interface is not changed, the GUI is not either!
only extra useful cvars and httpdownload etc.

keeping it oldschool so loading bars will still show if you download over http
it is ratecapped so you still get the waiting-feel (loading bar), i.e. to 80kB/s
you can change the ratecap if you don't want to wait, but then it is not the legit quake2 feel

the original client might take 10mins to download skins+maps, but this will only take i.e. ½min

IRC LOG of features to add:


- Better client than aprq2, q2pro, r1q2 , they are slow / buggy / change the UI so the game slows down/looks weird
- HTTP downloads via curl
   the original client might take 10mins to download 2 custom skins + 1 map
    keep it oldschool, it should take at least ½min to download everything, show the download progress bar
    add a cvar with the capped rate, so the user can decide if he wants to wait to get the full game effect
    cl_http_ratecap 0 for no waiting
    cl_http_ratecap 80 for 80kB/s
- More oldschool features
- Fix config.cfg save bug
   it never saves it, if I run a mod i.e.
   I think there should be an option
   to keep saving it to just 1 config.cfg
   no matter if you got 10000 mods

  cl_enforce_cfg 1  , always save all mods config as baseq2/config.cfg 
     and always use baseq2/config.cfg no matter the mod played, if this var is set
     
  ((should also be choseable in multiplayer menu and on by default))
  
  "writeconfig" cmd, like in 3.24 , to enforce writing a config
- Remove stufftext ability with cvar
- Avi file output in 3rd person
- Masterserver browser like in Quake III arena

Specific cross platform notes:
There is NO Makefile, if you are a linux/mac user you have to make a Makefile yourself/use Zoid's
   this might be added later....
   
   (the zquake2.exe can be run perfectly in wine though under linux, so you don't have to compile)
