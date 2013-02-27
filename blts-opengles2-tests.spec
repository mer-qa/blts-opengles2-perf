Summary: BLTS OpenGL ES2 test set
Name: blts-opengles2-tests
Version: 0.0.21
Release: 1
License: GPLv2
Group: Development/Testing
URL: http://wiki.meego.com/Quality/TestSuite/OpenGLGLES_Performance_Test_Specification
Source0: %{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root
BuildRequires: libbltscommon-devel libX11-devel libXdamage-devel libXcomposite-devel
BuildRequires: libXext-devel pkgconfig(egl) pkgconfig(glesv2)
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
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%doc README COPYING
%{_prefix}/*
