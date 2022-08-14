main: main.cpp gme.a
	g++ main.cpp gme.a -lportaudio -o main -I/opt/homebrew/include -L/opt/homebrew/lib

gme.a: gme/Nsf_Emu.cpp
	g++ -c gme/Blip_Buffer.cpp gme/Classic_Emu.cpp gme/Data_Reader.cpp gme/Effects_Buffer.cpp gme/gme.cpp gme/Gme_File.cpp gme/M3u_Playlist.cpp gme/Multi_Buffer.cpp gme/Music_Emu.cpp gme/Nes_Apu.cpp gme/Nes_Cpu.cpp gme/Nes_Fme7_Apu.cpp gme/Nes_Namco_Apu.cpp gme/Nes_Oscs.cpp gme/Nes_Vrc6_Apu.cpp gme/Nsf_Emu.cpp
	ar rvs gme.a *.o
	rm -f *.o

clean:
	rm gme.a main
