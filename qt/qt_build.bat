rmdir generated /s /q
mkdir generated
uic ../qt/hunter.ui -o ../qt/generated/ui_hunter.h
moc ../src/inc/hunter.h -o ../qt/generated/moc_hunter.cpp
moc ../src/inc/streamer.h -o ../qt/generated/moc_streamer.cpp
moc ../src/inc/player.h -o ../qt/generated/moc_player.cpp