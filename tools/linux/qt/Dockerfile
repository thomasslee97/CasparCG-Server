FROM casparcg/base:1.3.0 as BUILD

RUN wget https://raw.githubusercontent.com/benlau/qtci/master/recipes/install-qt -O /usr/bin/install-qt && chmod +x /usr/bin/install-qt
RUN wget https://raw.githubusercontent.com/benlau/qtci/master/bin/extract-qt-installer -O /usr/bin/extract-qt-installer && chmod +x /usr/bin/extract-qt-installer

ENV QT_CI_PACKAGES=qt,qt.5101,qt.5101.gcc_64
RUN install-qt 5.10.1 /opt/qt5

FROM scratch
  COPY --from=BUILD /Qt/5.10.1/gcc_64 /opt/qt5
