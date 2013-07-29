Summary: BLTS OpenGL ES2 test set
Name: blts-opengles2-tests
Version: 0.0.22
Release: 1
License: GPLv2
Group: Development/Testing
URL: https://github.com/mer-qa/blts-opengles2-perf
Source0: %{name}-%{version}.tar.gz
BuildRequires: libbltscommon-devel
BuildRequires: pkgconfig(wayland-client)
BuildRequires: pkgconfig(wayland-egl)
BuildRequires: pkgconfig(egl)
BuildRequires: pkgconfig(glesv2)
%define _prefix /opt/tests/%{name}

%description
This package contains functional and performance tests for OpenGL ES2.

%prep
%setup -q

%build
./autogen.sh
%configure --prefix=%{_prefix} --libdir=%{_prefix}
make

%install
make install DESTDIR=$RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%doc README COPYING
%{_prefix}/*
