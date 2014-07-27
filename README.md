nofuzz-quake2
=============

No fuzz quake2.exe for windows (keep quake2 oldschool features, keep legacy features, add http download etc.)


Alternative to q2pro,r1q2 - decided to make a quake2 client that is as close to the original as possible
while only adding essential features, like http download, avi output etc.
the user interface is not changed, the GUI is not either!
only extra useful cvars and httpdownload etc.

keeping it oldschool so loading bars will still show if you download over http
it is ratecapped so you still get the waiting-feel (loading bar), i.e. to 80kB/s
you can change the ratecap if you don't want to wait, but then it is not the legit quake2 feel

the original client might take 10mins to download skins+maps, but this will only take i.e. ½min

IRC LOG of features to add:

[23:45:10] <mio-> maybe I should develop a quake2.exe that really works , instead of that q2pro crap, r1q2 crap , now that I run windows
[23:45:20] <mio-> just need a faster pc so I can compile, this one doesnt have much diskspace either
[23:45:29] <mio-> its much too slow, everything takes 15seconds to open
[23:46:07] <mio-> if you wanna help with that let me know, we could put it on github, make a windows repository, then we -don-t have to include a makefile
[23:46:24] <mio-> and copy it all to a linux repos too, but the linux user will have to make a makefile/use the default one
[23:46:38] <mio-> but I would focus on the windows one then
[23:47:52] <mio-> add r1q2 proto number/version so you can connect anywhere, add http downloads(libcurl), add more oldschool features (no eyecandy crap, just stability and oldschool), fix config.cfg save bug, remove stufftext ability with a cvar
[23:48:00] <mio-> last one you have already coded
[23:48:07] <mio-> and .avi output too it should have
[23:48:13] <mio-> 3rd person .avi output
[23:48:26] <mio-> I got a thirdperson dll code, and the cmds to make thirdperson angles that look good
[23:48:44] <mio-> 3rd person videos are much cooler than 1st person, and it should remove crosshair automatically when aviout
[23:49:11] <mio-> might add masterserver browsing too
[23:49:18] <mio-> like in q3... q3 can do that
[23:50:45] <mio-> original quake2.exe is clearly not optimal
[23:50:53] <mio-> it takes 3-4 mins to download just one skin sometimes
[23:50:55] <mio-> over udp
[23:51:30] <mio-> and same for a map, so 2 custom skins + 1 map = maybe 10min waiting time to join
[23:52:06] <mio-> but maybe keep it retro so the user still gets that downloading feel, the download progress bar
[23:52:13] <mio-> force it to max download at i.e. 80kB/s :P
[23:52:23] <mio-> so it doesn't just download it all in 0.1seconds
[23:52:33] <mio-> it should keep the original quake2 feel
[23:52:39] <mio-> so the download progress bar should be there!
[23:53:12] <mio-> put the caprate in a cvar, then the user can change if he is not satisfied with short waiting time
