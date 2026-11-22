# Changelog

## 2.1.4

- Reduced the PRO surround selector to exactly `ALL`, `L+R`, `C`, `LFE` and `Ls+Rs`; clicking a meter strip selects its group.
- VU needles now follow the selector and use the shared 5.1 channel palette; loudness and true peak stay on the fixed ITU downmix with LFE excluded.
- Fixed ALSA/PipeWire 5.1 channel order in the Linux standalone (`L R Ls Rs C LFE` -> SMPTE `L R C LFE Ls Rs`); the DAW/VST3 path is unchanged.
- Raised the view snapshot to version 20 and migrated older selector states; updated both READMEs.
