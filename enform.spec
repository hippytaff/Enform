%{!?_rel:%{expand:%%global _rel 0.enl%{?dist}}}

Summary: The Enform window manager
Name: enform
Version: 0.0.2
Release: %{_rel}
License: BSD
Group: User Interface/Desktops
URL: http://www.enform.org/
Source: ftp://ftp.enform.org/pub/enform/%{name}-%{version}.tar.gz
Packager: %{?_packager:%{_packager}}%{!?_packager:Michael Jennings <mej@eterm.org>}
Vendor: %{?_vendorinfo:%{_vendorinfo}}%{!?_vendorinfo:The Enform Project (http://www.enform.org/)}
Distribution: %{?_distribution:%{_distribution}}%{!?_distribution:%{_vendor}}
Prefix: %{_prefix}
#BuildSuggests: xorg-x11-devel, XFree86-devel, libX11-devel
BuildRequires: efl-devel >= 1.7.10, edje-devel, edje-bin
BuildRequires: eeze-devel
Requires: efl >= 1.7.10
BuildRoot: %{_tmppath}/%{name}-%{version}-root

%description
Enform is a window manager.

%package devel
Summary: Development headers for Enform. 
Group: User Interface/Desktops
Requires: %{name} = %{version}
Requires: efl-devel, edje-devel

%description devel
Development headers for Enform.

%prep
%setup -q

%build
%{configure} --prefix=%{_prefix} --with-profile=FAST_PC
%{__make} %{?_smp_mflags} %{?mflags}

%install
%{__make} %{?mflags_install} DESTDIR=$RPM_BUILD_ROOT install
test -x `which doxygen` && sh gendoc || :
rm -f `find $RPM_BUILD_ROOT/usr/lib/enform -name "*.a" -print`
rm -f `find $RPM_BUILD_ROOT/usr/lib/enform -name "*.la" -print`

%clean
test "x$RPM_BUILD_ROOT" != "x/" && rm -rf $RPM_BUILD_ROOT

%post
/sbin/ldconfig

%postun
/sbin/ldconfig

%files
%defattr(-, root, root)
%doc AUTHORS COPYING README
%dir %{_sysconfdir}/enform
%config(noreplace) %{_sysconfdir}/enform/*
%config(noreplace) %{_sysconfdir}/xdg/menus/enform.menu
%{_bindir}/enform
%{_bindir}/enform_*
#%{_bindir}/eap_to_desktop
%{_libdir}/%{name}
%{_datadir}/%{name}
%{_datadir}/locale/*
%{_datadir}/xsessions/%{name}.desktop
%{_datadir}/applications/enform_filemanager.desktop

%files devel
%defattr(-, root, root)
%{_includedir}/enform
%{_libdir}/pkgconfig/*.pc

%changelog
