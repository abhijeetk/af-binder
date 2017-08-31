#
# spec file for package app-framework-binder
#

%define _prefix /opt/AGL
%define __cmake cmake

Name:           agl-app-framework-binder
Version:        2.0
Release:        0
License:        GPL-2.0
Summary:        app-framework-binder
Group:          Development/Libraries/C and C++
Url:            https://gerrit.automotivelinux.org/gerrit/#/admin/projects/src/app-framework-binder
Source:         %{name}_%{version}.orig.tar.gz
#BuildRequires:  gdb 
BuildRequires:  pkgconfig(libmicrohttpd) >= 0.9.55
BuildRequires:  make
BuildRequires:  cmake
BuildRequires:  pkgconfig(libsystemd) >= 222
BuildRequires:  pkgconfig(openssl)
BuildRequires:  pkgconfig(uuid)
BuildRequires:  libgcrypt-devel
BuildRequires:  pkgconfig(gnutls)
BuildRequires:  pkgconfig(json-c)
BuildRequires:  file-devel

BuildRoot:      %{_tmppath}/%{name}-%{version}-build

%description
app-framework-binder

%package devel
Summary:        app-framework-binder-devel
Group:          Development/Libraries/C and C++
Requires:       %{name} = %{version}
Provides:       pkgconfig(%{name}) = %{version}

%description devel
app-framework-binder-devel

%prep
%setup -q

%build
export PKG_CONFIG_PATH=%{_libdir}/pkgconfig
%cmake  -DAGL_DEVEL=1 -DINCLUDE_MONITORING=ON
%__make %{?_smp_mflags}


%install
[ -d build ] && cd build
%make_install

mkdir -p %{buildroot}%{_sysconfdir}/profile.d
cat << EOF > %{buildroot}%{_sysconfdir}/profile.d/AGL_%{name}.sh
#----------  AGL %{name} options Start ---------" 
# Object: AGL cmake option for  binder/bindings
export LD_LIBRARY_PATH=%{_libdir}:\${LD_LIBRARY_PATH}
export LIBRARY_PATH=%{_libdir}:\${LIBRARY_PATH}
export PKG_CONFIG_PATH=%{_libdir}/pkgconfig:\${PKG_CONFIG_PATH}
export PATH=%{_bindir}:\$PATH
#----------  AGL options End ---------
EOF

%post

%postun

%files
%defattr(-,root,root)
%dir %{_bindir}
%{_bindir}/afb-client-demo
%{_bindir}/afb-daemon
%{_bindir}/afb-genskel
%{_bindir}/afb-exprefs
%{_bindir}/afb-json2c

%dir %{_libdir}
%dir %{_libdir}/afb
%{_libdir}/afb/afb-dbus-binding.so
%{_libdir}/afb/authLogin.so
%{_libdir}/libafbwsc.so.1
%{_libdir}/libafbwsc.so.1.0

#app-framework-binder demo
%{_libdir}/afb/demoContext.so
%{_libdir}/afb/demoPost.so
%{_libdir}/afb/helloWorld.so
%{_libdir}/afb/tic-tac-toe.so
%{_libdir}/afb/monitoring/*
%{_sysconfdir}/profile.d/AGL_%{name}.sh

#app-framework-binder monitoring
%dir %{_libdir}/afb/monitoring
%{_libdir}/afb/monitoring/*

%files devel
%defattr(-,root,root)
%dir %{_prefix}
%{_libdir}/libafbwsc.so
%dir %{_includedir}
%dir %{_includedir}/afb
%{_includedir}/afb/*.h
%dir %{_libdir}/pkgconfig
%{_libdir}/pkgconfig/*.pc
