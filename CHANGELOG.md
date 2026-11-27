# Changelog

## 2.2.1

- Rounded popup menus, submenus and tooltips with a pill highlight, matching the rest of the interface.
- Clipped the correlation-bar fill to its rounded track and rounded the spectrum/spectrogram corners.
- No changes to the discrete 5.1 routing, BS.1770 weighting or LFE handling.

## 2.2.0

- Added the same transparent fullscreen FFT toggle in the spectrum corner: FFT fills the window while the right rail stays available, and the same corner or `Escape` restores the normal layout.
- Raised the LFE meter trim only for real signal, so silence stays at the `-60 dB` floor.

## 2.1.4

- Reduced the PRO surround selector to exactly `ALL`, `L+R`, `C`, `LFE` and `Ls+Rs`; clicking a meter strip selects its group.
- VU needles now follow the selector and use the shared 5.1 channel palette; loudness and true peak stay on the fixed ITU downmix with LFE excluded.
- Fixed ALSA/PipeWire 5.1 channel order in the Linux standalone (`L R Ls Rs C LFE` -> SMPTE `L R C LFE Ls Rs`); the DAW/VST3 path is unchanged.
- Raised the view snapshot to version 20 and migrated older selector states; updated both READMEs.
