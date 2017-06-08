#
# spec file for package app-framework-binder
#

Name:           app-framework-binder
Version:        2.0
Release:        0
License:        GPL-2.0
Summary:        app-framework-binder
Group:          Development/Libraries/C and C++
Url:            https://gerrit.automotivelinux.org/gerrit/#/admin/projects/src/app-framework-binder
Source:         %{name}_%{version}.orig.tar.gz
#BuildRequires:  gdb 
BuildRequires:  pkgconfig(libmicrohttpd) >= 0.9.54
BuildRequires:  make
BuildRequires:  cmake
BuildRequires:  pkgconfig(libsystemd)
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

%description devel
app-framework-binder-devel

%prep
%setup -q

%build
%cmake
%__make %{?_smp_mflags}


%install
[ -d build ] && cd build
%make_install

%post

%postun

%files
%defattr(-,root,root)
%{_bindir}/afb-client-demo
%{_bindir}/afb-daemon
%{_bindir}/afb-genskel
%{_bindir}/afb-exprefs
%{_bindir}/afb-json2c

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

%files devel
%defattr(-,root,root)
%{_libdir}/libafbwsc.so
%dir %{_includedir}/afb
%{_includedir}/afb/*.h
%{_libdir}/pkgconfig/*.pc
