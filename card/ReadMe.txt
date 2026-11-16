NetSurf for the SHARP Brain

NetSurf, a small web browser, built for the SHARP Brain PW-SH1, PW-SH2 and
other Windows CE 6.0 models.

Installing
NetSurf runs from a microSD card set up with brainsurgeon
(https://github.com/grimace-sdk/brainsurgeon).

1. Unzip the NetSurf zip at the root of the card. It adds the NetSurf folder
   and a shortcut in pwsh2-boot/Programs.
2. Put the card in the Brain and start it. Choose Start > Programs > NetSurf.

The Brain needs a network connection, from Brainwave or USB RNDIS.

Using it
- Tap the address bar, type an address and press Enter.
- Tap links to follow them. The arrow keys and Page Up/Down scroll the page.
- The buttons along the top are Back, Forward, Reload, Stop and History.

Notes
- There is no JavaScript.
- Secure sites use TLS 1.2 or older. Sites that only offer TLS 1.3 will not
  open.
- Certificates are checked against the Brain's clock. If secure sites fail,
  check the date and time.
- Cookies are kept in Cookies, beside AppMain.exe. NetSurf.log there records
  warnings.

Licences
NetSurf is free software under the GNU General Public License, version 2. The
licences of NetSurf and the libraries it uses are in the licences folder. The
source code is at https://github.com/Gadgetoid/netsurf-ce.
