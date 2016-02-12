MAINTAINERCLEANFILES = Makefile.in
filesdir = $(sysconfdir)/enform
files_DATA =

if INSTALL_SYSACTIONS
files_DATA += sysactions.conf
endif

menusdir = $(sysconfdir)/xdg/menus
menus_DATA =

if INSTALL_ENFORM_MENU
menus_DATA += enform.menu
endif

EXTRA_DIST = $(files_DATA) $(menus_DATA)
