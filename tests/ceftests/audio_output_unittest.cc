// Copyright (c) 2018 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/base/cef_callback.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/test_handler.h"
#include "tests/gtest/include/gtest/gtest.h"
#include "tests/shared/browser/client_app_browser.h"

using client::ClientAppBrowser;

// Taken from:
// http://www.iandevlin.com/blog/2012/09/html5/html5-media-and-data-uri/
#define AUDIO_DATA                                                             \
  "data:audio/"                                                                \
  "ogg;base64,T2dnUwACAAAAAAAAAAA+"                                            \
  "HAAAAAAAAGyawCEBQGZpc2hlYWQAAwAAAAAAAAAAAAAA6AMAAAAAAAAAAAAAAAAAAOgDAAAAAA" \
  "AAAAAAAAAAAAAAAAAAAAAAAAAAAABPZ2dTAAIAAAAAAAAAAINDAAAAAAAA9LkergEeAXZvcmJp" \
  "cwAAAAACRKwAAAAAAAAA7gIAAAAAALgBT2dnUwAAAAAAAAAAAAA+"                       \
  "HAAAAQAAAPvOJxcBUGZpc2JvbmUALAAAAINDAAADAAAARKwAAAAAAAABAAAAAAAAAAAAAAAAAA" \
  "AAAgAAAAAAAABDb250ZW50LVR5cGU6IGF1ZGlvL3ZvcmJpcw0KT2dnUwAAAAAAAAAAAACDQwAA" \
  "AQAAAGLSAC4Qdv//////////////////"                                           \
  "cQN2b3JiaXMdAAAAWGlwaC5PcmcgbGliVm9yYmlzIEkgMjAwOTA3MDkCAAAAIwAAAEVOQ09ERV" \
  "I9ZmZtcGVnMnRoZW9yYS0wLjI2K3N2bjE2OTI0HgAAAFNPVVJDRV9PU0hBU0g9ODExM2FhYWI5" \
  "YzFiNjhhNwEFdm9yYmlzK0JDVgEACAAAADFMIMWA0JBVAAAQAABgJCkOk2ZJKaWUoSh5mJRISS" \
  "mllMUwiZiUicUYY4wxxhhjjDHGGGOMIDRkFQAABACAKAmOo+"                           \
  "ZJas45ZxgnjnKgOWlOOKcgB4pR4DkJwvUmY26mtKZrbs4pJQgNWQUAAAIAQEghhRRSSCGFFGKI" \
  "IYYYYoghhxxyyCGnnHIKKqigggoyyCCDTDLppJNOOumoo4466ii00EILLbTSSkwx1VZjrr0GXX" \
  "xzzjnnnHPOOeecc84JQkNWAQAgAAAEQgYZZBBCCCGFFFKIKaaYcgoyyIDQkFUAACAAgAAAAABH" \
  "kRRJsRTLsRzN0SRP8ixREzXRM0VTVE1VVVVVdV1XdmXXdnXXdn1ZmIVbuH1ZuIVb2IVd94VhGI" \
  "ZhGIZhGIZh+"                                                                \
  "H3f933f930gNGQVACABAKAjOZbjKaIiGqLiOaIDhIasAgBkAAAEACAJkiIpkqNJpmZqrmmbtmi" \
  "rtm3LsizLsgyEhqwCAAABAAQAAAAAAKBpmqZpmqZpmqZpmqZpmqZpmqZpmmZZlmVZlmVZlmVZl" \
  "mVZlmVZlmVZlmVZlmVZlmVZlmVZlmVZlmVZQGjIKgBAAgBAx3Ecx3EkRVIkx3IsBwgNWQUAyAA" \
  "ACABAUizFcjRHczTHczzHczxHdETJlEzN9EwPCA1ZBQAAAgAIAAAAAABAMRzFcRzJ0SRPUi3Tc" \
  "jVXcz3Xc03XdV1XVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVYHQkFUAAAQAACG" \
  "dZpZqgAgzkGEgNGQVAIAAAAAYoQhDDAgNWQUAAAQAAIih5CCa0JrzzTkOmuWgqRSb08GJVJsnu" \
  "amYm3POOeecbM4Z45xzzinKmcWgmdCac85JDJqloJnQmnPOeRKbB62p0ppzzhnnnA7GGWGcc85" \
  "p0poHqdlYm3POWdCa5qi5FJtzzomUmye1uVSbc84555xzzjnnnHPOqV6czsE54Zxzzonam2u5C" \
  "V2cc875ZJzuzQnhnHPOOeecc84555xzzglCQ1YBAEAAAARh2BjGnYIgfY4GYhQhpiGTHnSPDpO" \
  "gMcgppB6NjkZKqYNQUhknpXSC0JBVAAAgAACEEFJIIYUUUkghhRRSSCGGGGKIIaeccgoqqKSSi" \
  "irKKLPMMssss8wyy6zDzjrrsMMQQwwxtNJKLDXVVmONteaec645SGultdZaK6WUUkoppSA0ZBU" \
  "AAAIAQCBkkEEGGYUUUkghhphyyimnoIIKCA1ZBQAAAgAIAAAA8CTPER3RER3RER3RER3RER3P8" \
  "RxREiVREiXRMi1TMz1VVFVXdm1Zl3Xbt4Vd2HXf133f141fF4ZlWZZlWZZlWZZlWZZlWZZlCUJ" \
  "DVgEAIAAAAEIIIYQUUkghhZRijDHHnINOQgmB0JBVAAAgAIAAAAAAR3EUx5EcyZEkS7IkTdIsz" \
  "fI0T/M00RNFUTRNUxVd0RV10xZlUzZd0zVl01Vl1XZl2bZlW7d9WbZ93/d93/d93/d93/"      \
  "d939d1IDRkFQAgAQCgIzmSIimSIjmO40iSBISGrAIAZAAABACgKI7iOI4jSZIkWZImeZZniZqp" \
  "mZ7pqaIKhIasAgAAAQAEAAAAAACgaIqnmIqniIrniI4oiZZpiZqquaJsyq7ruq7ruq7ruq7ruq" \
  "7ruq7ruq7ruq7ruq7ruq7ruq7ruq7rukBoyCoAQAIAQEdyJEdyJEVSJEVyJAcIDVkFAMgAAAgA" \
  "wDEcQ1Ikx7IsTfM0T/"                                                         \
  "M00RM90TM9VXRFFwgNWQUAAAIACAAAAAAAwJAMS7EczdEkUVIt1VI11VItVVQ9VVVVVVVVVVVV" \
  "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV1TRN0zSB0JCVAAAZAAACKcWahFCSQU5K7EVpxiAHrQ" \
  "blKYQYk9iL6ZhCyFFQKmQMGeRAydQxhhDzYmOnFELMi/"                               \
  "Glc4xBL8a4UkIowQhCQ1YEAFEAAAZJIkkkSfI0okj0JM0jijwRgCR6PI/"                  \
  "nSZ7I83geAEkUeR7Pk0SR5/"                                                    \
  "E8AQAAAQ4AAAEWQqEhKwKAOAEAiyR5HknyPJLkeTRNFCGKkqaJIs8zTZ5mikxTVaGqkqaJIs8z" \
  "TZonmkxTVaGqniiqKlV1XarpumTbtmHLniiqKlV1XabqumzZtiHbAAAAJE9TTZpmmjTNNImiak" \
  "JVJc0zVZpmmjTNNImiqUJVPVN0XabpukzTdbmuLEOWPdF0XaapukzTdbmuLEOWAQAASJ6nqjTN" \
  "NGmaaRJFU4VqSp6nqjTNNGmaaRJFVYWpeqbpukzTdZmm63JlWYYte6bpukzTdZmm65JdWYYsAw" \
  "AA0EzTlomi7BJF12WargvX1UxTtomiKxNF12WargvXFVXVlqmmLVNVWea6sgxZFlVVtpmqbFNV" \
  "Wea6sgxZBgAAAAAAAAAAgKiqtk1VZZlqyjLXlWXIsqiqtk1VZZmpyjLXtWXIsgAAgAEHAIAAE8" \
  "pAoSErAYAoAACH4liWpokix7EsTRNNjmNZmmaKJEnTPM80oVmeZ5rQNFFUVWiaKKoqAAACAAAK" \
  "HAAAAmzQlFgcoNCQlQBASACAw3EsS9M8z/"                                         \
  "NEUTRNk+"                                                                   \
  "NYlueJoiiapmmqKsexLM8TRVE0TdNUVZalaZ4niqJomqqqqtA0zxNFUTRNVVVVaJoomqZpqqqq" \
  "ui40TRRN0zRVVVVdF5rmeaJomqrquq4LPE8UTVNVXdd1AQAAAAAAAAAAAAAAAAAAAAAEAAAcOA" \
  "AABBhBJxlVFmGjCRcegEJDVgQAUQAAgDGIMcWYUQpCKSU0SkEJJZQKQmmppJRJSK211jIpqbXW" \
  "WiWltJZay6Ck1lprmYTWWmutAACwAwcAsAMLodCQlQBAHgAAgoxSjDnnHDVGKcacc44aoxRjzj" \
  "lHlVLKOecgpJQqxZxzDlJKGXPOOecopYw555xzlFLnnHPOOUqplM455xylVErnnHOOUiolY845" \
  "JwAAqMABACDARpHNCUaCCg1ZCQCkAgAYHMeyPM/"                                    \
  "zTNE0LUnSNFEURdNUVUuSNE0UTVE1VZVlaZoomqaqui5N0zRRNE1VdV2q6nmmqaqu67pUV/"    \
  "RMU1VdV5YBAAAAAAAAAAAAAQDgCQ4AQAU2rI5wUjQWWGjISgAgAwAAMQYhZAxCyBiEFEIIKaUQ" \
  "EgAAMOAAABBgQhkoNGQlAJAKAAAYo5RzzklJpUKIMecglNJShRBjzkEopaWoMcYglJJSa1FjjE" \
  "EoJaXWomshlJJSSq1F10IoJaXWWotSqlRKaq3FGKVUqZTWWosxSqlzSq3FGGOUUveUWoux1iil" \
  "dDLGGGOtzTnnZIwxxloLAEBocAAAO7BhdYSTorHAQkNWAgB5AAAIQkoxxhhjECGlGGPMMYeQUo" \
  "wxxhhUijHGHGMOQsgYY4wxByFkjDHnnIMQMsYYY85BCJ1zjjHnIITQOceYcxBC55xjzDkIoXOM" \
  "MeacAACgAgcAgAAbRTYnGAkqNGQlABAOAAAYw5hzjDkGnYQKIecgdA5CKqlUCDkHoXMQSkmpeA" \
  "46KSGUUkoqxXMQSgmhlJRaKy6GUkoopaTUUpExhFJKKSWl1ooxpoSQUkqptVaMMaGEVFJKKbZi" \
  "jI2lpNRaa60VY2wsJZXWWmutGGOMaym1FmOsxRhjXEuppRhrLMYY43tqLcZYYzHGGJ9baimmXA" \
  "sAMHlwAIBKsHGGlaSzwtHgQkNWAgC5AQAIQkoxxphjzjnnnHPOSaUYc8455yCEEEIIIZRKMeac" \
  "c85BByGEEEIoGXPOOQchhBBCCCGEUFLqmHMOQgghhBBCCCGl1DnnIIQQQgghhBBCSqlzzkEIIY" \
  "QQQgghhJRSCCGEEEIIIYQQQggppZRCCCGEEEIIIZQSUkophRBCCCWEEkoIJaSUUgohhBBCKaWE" \
  "UkJJKaUUQgillFBKKaGUkFJKKaUQQiillFBKKSWllFJKJZRSSikllFBKSimllEoooZRQSimllJ" \
  "RSSimVUkopJZRSSgkppZRSSqmUUkoppZRSUkoppZRSKaWUUkoppaSUUkoppVJKKaWUEkpJKaWU" \
  "UkqllFBKKaWUUlJKKaWUSgqllFJKKaUAAKADBwCAACMqLcROM648AkcUMkxAhYasBABSAQAAQi" \
  "illFJKKTWMUUoppZRSihyklFJKKaWUUkoppZRSSimVUkoppZRSSimllFJKKaWUUkoppZRSSiml" \
  "lFJKKaWUUkoppZRSSimllFJKKaWUUkoppZRSSimllFJKKaWUUkoppZRSSimllFJKAcDdFw6APh" \
  "M2rI5wUjQWWGjISgAgFQAAMIYxxphyzjmllHPOOQadlEgp5yB0TkopPYQQQgidhJR6ByGEEEIp" \
  "KfUYQyghlJRS67GGTjoIpbTUaw8hhJRaaqn3HjKoKKWSUu89tVBSainG3ntLJbPSWmu9595LKi" \
  "nG2nrvObeSUkwtFgBgEuEAgLhgw+"                                               \
  "oIJ0VjgYWGrAIAYgAACEMMQkgppZRSSinGGGOMMcYYY4wxxhhjjDHGGGOMMQEAgAkOAAABVrAr" \
  "s7Rqo7ipk7zog8AndMRmZMilVMzkRNAjNdRiJdihFdzgBWChISsBADIAAMRRrDXGXitiGISSai" \
  "wNQYxBibllxijlJObWKaWUk1hTyJRSzFmKJXRMKUYpphJCxpSkGGOMKXTSWs49t1RKCwAAgCAA" \
  "wECEzAQCBVBgIAMADhASpACAwgJDx3AREJBLyCgwKBwTzkmnDQBAECIzRCJiMUhMqAaKiukAYH" \
  "GBIR8AMjQ20i4uoMsAF3Rx14EQghCEIBYHUEACDk644Yk3POEGJ+"                       \
  "gUlToQAAAAAAAIAHgAAEg2gIhoZuY4Ojw+"                                         \
  "QEJERkhKTE5QUlQEAAAAAAAQAD4AAJIVICKamTmODo8PkBCREZISkxOUFJUAAEAAAQAAAAAQQA" \
  "ACAgIAAAAAAAEAAAACAk9nZ1MABAAAAAAAAAAAPhwAAAIAAADItsciAQBPZ2dTAABAKgAAAAAA" \
  "AINDAAACAAAAi/k29xgB/4b/av9h/0j/Wv9g/1r/UP9l/1//"                           \
  "Wv8A2jWsrb6NXUc1CJ0sSdewtPbGlo1NaJI8UVTVUGRZipC555WVlSnnZVlZWVlZljm1c+"     \
  "zimE1lYRMrAAAAAEGChIyc4DjOGcNecpzj3e5eskWraU5OsZ1ma2tra+/"                  \
  "QoUNbkyPMXUZO1Skw1yh8+fLly+84juMURSFhhhyPx1EDqAmLBR0xchzH8XgcYYYknU5HIoc//" \
  "F1uAOa6rplb8brWAjo6AuBaCWnBu1yRw+9I6HTe9bomx3Ecj8eHR7jhpx2EJSwhwxKWDtHpSA/" \
  "hd+Q6Q/"                                                                    \
  "XNZeIut1JxXdd1FzPAbGI6kyYm6HQcNmEJi07r6Ojo6OjQ6XQdhMWCTscBwEd2xARjIprIJiYm" \
  "JiYyf2KCACDkOB6Px+O3AKDQkNscN32A7tIn3tm+wPdQiK1gI2FpTbSPWkfP39+nb29vT9+3/"  \
  "Y+8NdEAfA+OmQ6zRtfR0dHR8ahTR0fH4+PjY0dHx2ynx8dHgB8U/"                       \
  "i6fLaUnx1wT25MmJiYmJqYDACDTYdbodB2EQ+9aRwD+Nkw+"                            \
  "hfQxSPHBdvQ2TD5FpJFBCCtwtLsEMYc15nbtXNNdkgqHYiKRlIwAAABAlCZiYkIIiThNSRKhE8" \
  "KqUrnsJ2hxoZt4CRurX076XaZaxJetiVOHTp0a+PgINiJWq8VwfLk+"                     \
  "cITkeOQ14Y4rvOkFV5gNbxGwcVJTDea6zsoAASCExwDXWK1chON6pdVirqN3roR6RupwgcQ1uT" \
  "LXI+"                                                                       \
  "HyOoth7KQkYR7fAFOJv3TclGuuX2CS60rmmwgoZRIFU8icwlwDSea3MKrOGxMM1XtqaLgmDcCL" \
  "YEbscM8PuoIEXYE9Qj08y62k5aQRDimNrAslDCa0CL3XGSYaTW0Q2etDMZyiS435NgHG4HACkQ" \
  "xzYNnYqtvRwqPLDKAT1fRDd5KIJ45cOoeyA1FHC455K8BYpAAAZ2gMqDAOQPcz9/"           \
  "v3uTNAASBXhW/"                                                              \
  "+wqevLAUrnjUnS7YzOs8s+"                                                     \
  "bpwXYrKdoXXGjBgp10SlQ8A3jb0scTwUeAFrmtD70uMfSS4gJeZlUhIlNsKco2uXVeY2VWl6JR" \
  "DSAhW4jYAQCYAAJCXD9bEGgGxF1Oz2UgEAhOlC0q5pjzL3fxjlQcAAACAjx8bmMEYnbAb1U4nz" \
  "BE2MsOHLwGuHz8oUi2qnhqYoTAuZWUNo0sfSn2HJJcA1xVleDATYEDmjGsqfYuV1VW3dhdQ11Y" \
  "rko0xrJHM6qZIpxW2qPLKAiBzakFdDasdLWtAzpUaGbaUXhZzReGzLuS71zqMZIhap91418WyG" \
  "4stA5xvC5AWfdPC0KFnhug9EJ6h0yAGfs54rQNMjP2JYPT0RkeosWCnkZ1GGYvGLCMRrhdEj8C" \
  "h8OOUvsYFzIyKCO+"                                                           \
  "MNsuxBAyvGFnp9QwEhcblgg5xA7gRNLmyHjMwEAWu57SEt2AIXIbqDRCqICCh7QEAvBIAAACSr" \
  "VYG45afyShwcSuuNzIo4AUK/"                                                   \
  "1ZvfgUABf42jCVGeVhwqQpxQ99SBB86rGrhPqKsDImUIoPYFTmNXd0Vlks7U8FRjYAEAACYOUp" \
  "Uk8RSEAkCIWK0JOXukmSu2R1+iWGIWLBM+mt0Up2tqni9YR6/"                          \
  "b6aK70i+"                                                                   \
  "IV0EAzUMs4ZAYRQJwvNSInBWKJtFAgS9MiFgEEYDmIOZMK8E4h5xAegwErEGYWbSzKJ0E5mz+"  \
  "AozI2QYjutAsEbhzsrxtoHkIjxIZo4Ho/"                                          \
  "RpMMDTvsug986rhaceoIQiQucUUCJPKaOJwDKa0Y2kiRhjDxOG6EGJEyEhATLXC0j4qKckgkeE" \
  "ugjRA8B1MY8D3sIBr0kOVsvFwQTfLbj3ABCIMrHSLyQ4qbOdjCEK8gghdCNG3wyjgskAAICiE8" \
  "D3VkkAYPyxpQAAwPXlW2HA/I/NAfx2KQA+3q4MBYDE/"                                \
  "X+cAACyJJGwfpZxAP42TMkH+"                                                   \
  "0AOLxIzb8MUfZQRHR7AkSHICOeGMHdG55ULc3qMjEjSBwO0SAEAAAgZYhCWUSyoKAVKKKsQL5v" \
  "0RJaFKF0iIp4A4u42AYA9HEIPhlrCoWNiOrlxU6OmOcVsXyAWNWWyYvEg1fLKMHi1MRcAqZ6qs" \
  "KKPcJIoAfWgjkIjWXkBzZLBQ2X0djWBHvsi6aIQ6rQmZ50vcrgEuGNleEwUBA1WpKJiZkbhShj" \
  "a5TrjZ8uHdL4p6sJpn0748t/"                                                   \
  "4Ky401MB4FwAw6vRWc8BMAjnySEYJoc5+"                                          \
  "VpmHtCG9622e9msJozQgHQ18GB16oycX3odS6siozeCNd2g8ow/"                        \
  "jDOloAgg3GK9JhkfU4FAwCLci2kD8KNGqMLrinHR6yujhyHcCArjgYYwpicBMtEILRJRmAK8nc" \
  "HC0MBHPNnh8fASAue68jrwrIuG/vZnupwGA/"                                       \
  "v5t2CyABGSAA942LF5E8x+"                                                     \
  "NWB4kPtqGxYto9scgpxdJx11V1ABBREREEsTMk6t0u1QyNV3MRCIdAgAA4C6ZGSzQBMyUmAhNp" \
  "RgtYEkrCYlUtqSPpYbpbf2LmAYxxcEodhZD1DDVNE1z4VabMg0MUzksaBW5vkWD45q5luFKQ4x" \
  "Dl6XhA0w4GGKQyRZwjQbX8Y1Q1y10x21clDHA4EPjADidlLiWSmXCUjIzk18yZHjkFeGU5hOws" \
  "jSXKma1g2NpIJmVsRIyHQb04fcMMljQLC4eVrUpAsBbR2P80rIFh7xkaD+"                 \
  "qQRJAhCF6amxRZzQLYCgAwGHk4tTxhp46UZ1GR01UxokEw3RgDR8aYLLBhBVWdRfXkdlKNjRIn" \
  "tIN5WpsAhOGYW3WNE6ychBocusS6He+3SoISm3RZ/"                                  \
  "ity3SkcDrh1O3GnqUWeMII2FsBAACOXK+ZAayOaQAAAMDr9fgQAAAQpgyQURIsAD43bNUE+"    \
  "YGaVgonZW7YqgtmBDX1FCdVPNoASzrC7pwh6GrXnSTOhZliQkJITZQjAQAAEBayiEAASkSEEhU" \
  "nAlqMIl4iEBUvFwr9KaKiwRKlIixCWBS0QJTtFQwxraBqER3lw4O5wsxcLRbgeBwXM5WRLq6EX" \
  "AII0SuonlAqIjsEkPAiYT5wvA6YhOuYQ5AkIsK4nA7DuAjJhDkrtwWLUBWrcSQhFlOglSQ3XXB" \
  "N6sIjyXFczCTkSo5X5hpGyJJ7MtDTHSJLkPBbpMBy2F1Er3PU7qKM5yMsFhDGeEooCKVk4zwFE" \
  "9pNnPoeCBgAYsJ9JsGHiYDTHHEpzKldb3scr1fvvJRLdLrQPhYwq7FljfBg/"               \
  "R6eNRZEBhQGYVX9QEdsDeG6qA6OAGjMBADgO6AIdQEAlaUfAAAAEM/"                     \
  "wQZz6AQAoK06aAAAAAGTherz+MgAAAMAHlAX1Uy0qAL42LNpFsx8HpWdH0LVh0T4i/"         \
  "zhIHWskPTKhHTrYuWpKV5KkjEUnpIYIJAEAAIhAIBSI0aIlSlQoChGhUKQQioEWUqJCUYeEmB9" \
  "8T5k2E/"                                                                    \
  "+DL3t1UQNjJmHEFP+"                                                          \
  "mrQyEabFWhEmubBFIrmAMZaYgjrBwHIfFMHkdTzDRuXSQqUP1KAKoAGJv6IC8ddTpGcyxIIyMN" \
  "kIHYlVoMSLCOpcBXBAKSR4Z5iJSHo/"                                             \
  "h+sSn1w1zcL2+t+"                                                            \
  "vggs6EoRb1YfQRvRtEfcwboET1ohMO0JBOF4wuclcSqndFBhgiPPWeAT6CUAZAZwylLtL7MC6Y" \
  "NRgRVn1YnQwIRYTuUl3EIdFbu47ru3l9uMJcud4JcxHQ6RSD7hqATRcNGv0Q6EIJPAUwANBRKP" \
  "xWAYAIcjoAAADwMf0lHqCej/"                                                   \
  "n7+"                                                                        \
  "9sAAAsWFtSVSDhHPrjcDBlFPWb95RQZfjds3cfIH5Ae5KTOd8PeYkz2Q8BDwXYfeeK0w1i5SQT" \
  "t1sByrhw9LiVZHOVBEgEAAKDECjFxMWamy8tFabpcQBMlLIwymhKDW/"                    \
  "motIQ8H8rNy3MJzBhFOsIQ0gAhTIG23swQ9rJa+"                                    \
  "Vo1kgrfbpgarwknpUFFUXhkAZnUyFzo9nYLTpTQKNarPtd46sfSmS7CSgl1VkzAxOFWowvyqwi" \
  "yQM9Io9rMVtMNIw0N6duZoR3zWRYFWWQhVKbjuF7WnVTZYY8CY4e4jCT93sfJJACJBRpY1l6NS" \
  "QDEmmzDBPqhSdNmLPURTE4SPg9PiIuMSJ5SGvOh8fQAYFSRR2T4XYMZETqDnihEsuKtxoSvzEQ" \
  "OhhXKl2oCK1LfMJe11t/NvT/"                                                   \
  "EuCdvlaj1JXsG+"                                                             \
  "sLnIEHNJtDRjXHAGhudngIATJQBHPC5X0dy8cgAwAAAnP3s8A77BoAGigofEvUvtSQzzckL3lz" \
  "5SgA+N2xVxqYfiG2hQDRxw5p0TP7RUFYed2WZkTJkEIIMG+"                            \
  "ax2rmxQ086qaIcOihPIgEAQAgBQniAcIAZEPPwclRAKFomKe7hVnULf3/"                  \
  "PrQ43ubI48InJ1po2TLadA0rEUDVUsVMxwBjVmBC0w250lKGkDmEFeevFhBnCXCZDGDrECQYkg" \
  "QebcT2OKzNKQDKHqgnDSkN6oiMCsInFdb1GjTwpqw4J2QmJpC4LKoDrLwrHbc/"             \
  "zI95E+Ki0iHd+wOuhI5EXB7E4UwUkRwKwRFLVul6uHiATCy54aCgNFdC7/"                 \
  "pjGcUyYMJw4OrgMxutRSL3lRBFhhNE7tAhCI0BJWCOb1aJpVEbyCMmnhOosy5XrmptCmDLAOhf" \
  "11KO/3ahqxOhRJZwP1/"                                                        \
  "AEZkTAcQBMVwwAAu8E9KzWgEiZCJPhBQAAMJcCAHp1D6QSAAAAzKfbAVCHthYLAAAAlQI8macA" \
  "IFQ+AD43bFXEXvzAuvFRN2xdx24/"                                               \
  "FOvOfNzVZYRMMqQMOWHNBpJ27Ry92YXHdDyJgHJYAAAAPxIzmSBiki6amEBSVBYqBTFapoRCod" \
  "BcQooSE4rQEqQLIYHXj79ceaxwo/D71/"                                           \
  "JVcaMMtwDAHFyFJLmS6+"                                                       \
  "hmgeso9zEQShTggBx3gco4QjLABeTBMDMltQ8Brgk7g1kxLtIOwrwCEwhsUeMKC7rG5ArX5HgN" \
  "fDrOIsfjlFavcc2EnBx3IoaOFs0aHKEuK26Jo1oGKpcKCVPHpO3dn7yJ0X8xXC73Lnd26CXLjg" \
  "MtCjN5hCFThAQ2oSFtOqOVGSDuE32hhYk4jsTlovpQozGwRbDa4UortUhDwkUAGGOR6jFYw2wX" \
  "FJWsYBDy4MgWAXCbVTMEAABQEJ9LgCwH3xYAAIBMa6ZNaQCgAcBTJ/"                     \
  "x3AOj3+wDAcLwer2MAAAAy2ACgwXf8bXKBCjY2xMkkMx+KLi9G1SRfQ+"                   \
  "pkrPnA2nUmeyDHu80EQVKEELbtjomYPTNESOfauaokPSZJA0UIkQAAYM42+guCLGQ+ns1y+"    \
  "KgSupXRApdXJEpi4pSLDJJKRcndw82tg0wkAgPrVMAMn+iQ3W/huqg/"                    \
  "Dn51VgoLfPn9gVTIoESUeFyxOCk/hSsDF5NZW70MrmNVrOoWZlo5bmhxTDLHOHTC3u/"        \
  "ymKjiOlaAEIzU6EcUCYPLmDnmVK1esZjWOPXKWdXtVr01YJGhu4TiVoJMBjheB9dxCXLovTEEh" \
  "RLGaUp2Y+m+"                                                                \
  "rAscdQDonFbotozWUeP0Lop2ZioVzASyLqcOHV0lOE9nZ1MAAUBTAAAAAAAAg0MAAAMAAABNPA" \
  "GzHHBCSXWA/4hFeXd4eHD/Tf9a/0z/RP9T/1T/"                                     \
  "VP80TOZAm1Nx5OLKuqg4gORtwnDFBzFFjQzs0XZdrDdKYSDmSjid4LQfDhKi5+"             \
  "h240ACA0CNW3cUDABUawBGbEDXTQBrAQCVA+gxJAeAooIVRYmfpDQ/"                     \
  "ADDPAJiZPJ4CKL0VwmKNDgAAABB4wcksgAwAnP100SFYKhzvrKcPFkH6ob/"                \
  "mUaXIRDBTcgHEGFi5xJcO3laj2nwCDrhVGxPvuosRTqeLwWlAfYSrIzNoeauADicEfPVoAlDqQ" \
  "aRFq+"                                                                      \
  "snciCkfCHS4qa8or0MBDMmF1j6cY1Cm4iGhjfGaeO3aieOGe5NDbGwDjMMcczOSPXRFkE9H8+"  \
  "2tivg+AwTAGQNEQxiLnIquGjWEMUgZgkKXsxZc5FSaqfd3Uw1z81sIB8+X5WJ4h5VVZ9m4+"    \
  "6i1buron8SP34vySmxQ7qLMCG0QEz0/"                                            \
  "dT7XwCo2VZgmg6XM5ywWqdMz5QJ8fsMtqXl9ASTOGbvISIWw86dzoFrOPK5YYp0AJQNE0WyW7g" \
  "QLT5gX0YMCEhLFyIFZx7T9tdYUcZB0VtJu6JumJQl5fBOSLhW37S5VXs34jK1Jk6VT/"        \
  "x83HsZZHFaDLQov1dP9gNAMvmItMzF99N26+QM0xzGyd1NG3vx7cvGMQe2Flvf/"            \
  "nbSajF9W32brvVMk+7b9uwetObQFh8AUjYUlVb4B2Kq5oLCGBuKhIP8byZy6k0Fy/"          \
  "P7VLTPiCKAqKtn1I4yy4MRVYUi94hYM9q163TiXMVijoCQBlgBIAEAAGyfygLXGCzgsKJUvzv9" \
  "XsNFCjlU+yZdS4D2WD4dm8oyL0+5TdC9AAAAAOSQ49//"                               \
  "kCTDIgMwAI6dPc3tnLUcLWWKMTh3gAtGuRBrX8HcVR2OmbiORPgtuEJZSZiw4OIuJlOhrDgCWN" \
  "qTvazq8R1kGqXJdTJxtXJKFMGgpws9wcyEk9FRCbfVYK7HpfDKt6iRl2yPW3hTVjWw8HljZcsx" \
  "BrKd+rDCSQCAGKtxnRWOVybT4lpXTXidEUdPVgF4UL9swRV23u0ZD+"                     \
  "F0MRKXIawEFIB1ekIgDYGbwsMKU+"                                               \
  "0ozKo9Z2TkC0c3gMxmkYQbJsxQOzYjDyQkIuv7bsFeisuWiRwzYCSRqHXOjQg98QZbtYEYxjGc" \
  "QcARkID2pQDgwPm9ngRunDrVrqxYyYraKzL4/"                                      \
  "nJO3t3aACCBuWOB5ax1sNREyTiHA2wFfRZkaS4gkWMr6LMg2DOhYKapgsoCSDNCBtw9OxjKFWD" \
  "CykgduoxFYqpga3WXZEJdsKgHsYBJTARBmHkdFKsAQCmnXnwROSE7Hv6llTQVfDiNLyIjZBf2/" \
  "ksraS74cBrfqkTCRD5PUtVGNNMKXBQCXNlcVlBc3JeFFaQQFSmXY1K0woOGWAXocnERodAlpA6" \
  "qrJyaaTkB4Jizva1jptWWCYvjdk79BZjTT5tmqsXOxhCQCDJiPlKf74sLJSRcEWkl5I7tv9mpY" \
  "BrTuCLSSsgd27/"                                                             \
  "ZqWCapv0HIOTKyCpCDKk6j0huPhQVAAtocQIRWkiGIAYtJsnw9BAjYl4xuuSLUnHcx2tjuUQyZ" \
  "hTkDACHjsQhFltHamcRO8SwkekNAZhRrXYOp5uxrLd4XtEwbb8z73zWNpwRWZbbM239xM8kh+"  \
  "uMyLLcnj0cip8LZsytqkYyyYqoriqQu5FSj1dkFaJAQSnL43E4lBKlTCgqQknRouwQXhEVEbg7" \
  "NHGEdOWrR9X0wRWbhOXu4rUZu3p+"                                               \
  "8FtsDIsThmlnNTCMKQ4thuOOKKuNZUbFxBOPz3vyLzwRKSzpbvMJu4IBp56ICCKJx9zGrmDDqb" \
  "OGUiFCT7KTkUeojozcJkSBhIijQiLK54mJE4GIKAsFkCwT8yUwIelyMU2EIAJRP8Vo0dPebkqG" \
  "aBR4ylMQA3+cdMJiYz7r7wm1Mez8OGaorRXUNFVncCyO9y+tTnwJTwwRNQtG5BcKZh5BQ70FA/" \
  "IHBTPXWRUpUnQexLSqkClyVlENHAqLYQOXD15ITCgkhBDxMhE3DxaKif+"                  \
  "YkFkgIqZ4SgmQMW2qv3wdmTb2rsVGrGpx6LhDJ2wcc4TF3saBL6Gd+vhJLbZdxG7+Z25+"      \
  "jQzaNYQuBh1BDYutlYCuIXQx6Aiq1aqVkFstWUNWFBEioowYI7tudxub1uSo3FXFSbHERiQSAg" \
  "IAAJAymXQJKSDADKEYiBhdQYsKPMVBM0UkxcSYEqFEpJDaskhhOmY6sBhiqCm2YmIRU7Cghi9b" \
  "QQwceq3qcWAxCjlmZiH6YF0HALHFRchc/"                                          \
  "IWBhRpjMYxFxrA4wLQKomKVxEB1ozEEOBlBjDOceDHHXZlryEFrZOCVme9uTSOkpQbDgxAsBoQ" \
  "6dBjBwWrXj4MpbNqgByzqAzrwgIMhAGSRUAI4ImPoj+4YhtEJ450rTr3Te+"                \
  "oKQ0EdEh0AuLwvoUgAWPT6ibwjn2ZyAcMQETA6C9hhQkdsorFi0RUGeqCkDB1Dd4A28ScCMQJw" \
  "uxH6QfQ/"                                                                   \
  "yMCNHCVggaJ1aR0BADDzeh0BwEQEwFyvuyQAAODD7zgA4LJkAL4G9D5GmqVR02oJnHPAa0DvY6" \
  "RZGjUtlsA5B3Rn1hAyRSpTRUo7mDuT66pYUiZ62NwCAxIAAACNtZcoBlgINkBEZhUtSYASZ1pI" \
  "CE0IAehCyA7TQgCCvQOL2OG46YRjoqO964AUEUVtMdWiMpMcF3OEHHwIyRNOXSSFFzOi1+"     \
  "tdFlz6MJEIiHfoFEGsBWqK2llRk+NFMszjmrkOYygQDQIEGAA+"                         \
  "HOSYuSZcnIoDMgcTuGgKb4MccORxwXUwMNM5NAQ+"                                   \
  "jEtmJnk8GAgTGCOIAaGRvV5HqNPpDSamPgLUZYEEhXR53c0QJzo6dJE7ZC1BLTrDgLgQUV0Y1Z" \
  "PI8bg60YUL6iMCM8fjQ0JCwjg9wzJDSCeIN5aeRgETm7ShW7pNB2jdSQAA1YwA0I0Y3ZIcAgAA" \
  "76QMFAAnAgAAoe4uy6WEMgFavwv68IdhABANMmTIIwAAAAA2XwkkAD43TD0Fe0BLqzkJe6wblp" \
  "6CPaCl1ZSEPR4R0mKQdrfEUNXOo1NjxXQ2M5EiggQAAKgQTLsoYbmoUISmWBTJoFKcEitjFhGy" \
  "QCgmQigBDQjFBZ4QZYPjTsiOMNVioDY+dqjHBzjITDKZHBoZrnA6iiCYOUJgwqFq+"          \
  "mDEu24kxORIYLjm+"                                                           \
  "PR65PUKYWASXsmhngAEHkhMCHx4ZAgAmbmSkb5JI4ZhgHh2YtpgAvN4XMOQmTMRjA6aHkCH+"   \
  "jA6FVDEAWqKgjhHlD9ZS4bOogVPLEZQBj2SnvQEZxim+"                               \
  "mvvsK7dgCUswSOc9JLIBK6oCaPbNTkHBedS9YS4cPG65rQAudgwO6bHoKsJYTTAMMK4AANmHVx" \
  "jTJ7Q6QEdEKZ7kpuib8EQLZAqQIkrdnQAGCnS4ojg7a1FusboazG0MBFHAGiOI6/"           \
  "H2pEcTw7goALeNkzFJ3EoxLSYAkePbcNUfBKHQk6LKVD0eNdQEYqElJIghn03CFyXqw6RHi66w" \
  "hQTcEMAAADsJRmSWJKEhBABRQi7i3sIRDxERcFEIE6J00wooiZWgLI6sqqoaaImA+"          \
  "AdUsJsHFq5eAwXHDQG1CCPzECqbQSSuXI9EjRqkAnfqWtrj2u6Y38kvY8UZgIRAAA9gwTCcT0O" \
  "QgBi8JqZ4zokjJNENj7UEZiDVyYzBMiQObjpNXAMqKGRYaiDoaEGQAgBHUEGgBAe0gRASDCOv8" \
  "NijCH5HbiKapzzQjhouA6MQFLiGN2g85EoRSdULj4MpRwDF0XuDnXc2ccYPjlmjuEBMwSjdvQR" \
  "SBM69k4wgteORhhoAuA7ZAMAoGH0pwAAKjI5AfAMBEZERr9LRwdR/f/"                    \
  "WBm+EjcfMXDOQBGAec8FkAsD/"                                                  \
  "AD4HLClF2mYmMaxmckRzw5pKSIdJDqsJz0ecGC0J59zOuW5Oko7MwuxAAgAAiAgkxSBKiREWij" \
  "MlLi6kaAERZYoCU2IQiGH1rfZGI+Y0W78s/"                                        \
  "lhtUBVDXdPJ0QA8ZxyJAOSovfLiMcBwMFwRSeciPbgYDXgEeBzcdBDURwTpIjNAHpnhYuYguTK" \
  "g3unQCUwgGeARIgDDzMxc4XEcTy23Ta6LzCOv15VPDK/"                               \
  "MXBnpOulLtVsXBVEAxmithYNQOPXElRBBXLggBE6XkyI0JMwyYBWGMgJ6q9BkKNxjaz1K1TsDI" \
  "oyDzmvPYKiekGJIEE67I4w5XhwXAANwweMUkuGCQRwqtQSNAIgJWVdWnOzt0Z73R6kl6IqhpHs" \
  "KQi20iNlJYKzVeep1jEBkLAHjLRq/"                                              \
  "UnJQIQC0AXjipN3gQHy1Bl5P4B0CMsRhT7d3VQAEcgya72QAvgY0MUVpGiE98OhpDWiijxBNIX" \
  "iQwwLyXWNlLSkJRJkI5pi5UlTXSULsyMwiIgAAACAhhBASEoKFEC4uToiQFkIImoYERQpM7Jzu" \
  "sG7HVhTTnGaL1ZaZDDK4tqNimK5HPlyfXuGYK5lrGByZSHqmoJCghDFsJCSBHqEkssICuV7MlS" \
  "HDpYuwyHgD4CnRASEjTEoCBEN8gwr0npGEhhokDMcVLq6oulqoGphjJoGEXHPku2pJQZM6zuIt" \
  "3uC7hCC6CBCqj0U4dfoOA9BhlLln1LEUBsTodNAZpm5GSuBRjpJ+R5Ek3R4mHnEpz/"         \
  "woPkxaVjIZVmODUztO1TAhxmg8DmYgD7CYCYOzn0EaEhI3AgAQI7XSEZoyRajTAI3kO0Dr0Emc" \
  "DwBtiE5koAsZFh2DbidOjEjVWTcA72J0RKGoGj76McwC3UjFxPOcEMipPa7jejAXHjcs0QdzQc" \
  "ovE8YYN0zJB/"                                                               \
  "lAyR8mjOEuq0UkyCICO4Spq9o1McwpI4rXhEUzgwAAAECCGVJIZgmWaCICIi6ghOIULUoEQhCA" \
  "6RKI+"                                                                      \
  "oEC7IVD7Oysin0xzEStE6bM1rjSClwzOYtrjuHTzMxr5i7CvI6LyeQY5piDQF4Mrwk51tqiBrC" \
  "gMwfXNWTmOI7HXIrwWSwhShx5b+"                                                \
  "Ii1xxZywAw5EiYRxgwNJIFRw6Ba3LAcaAiL64jny6u1yTMEI5h4Jh5EVCXHiZidw8ByIsUYNY6" \
  "nYnwnrh0YRwaCjh1jND6IoN+e8D4SOgefGSnBRCdK+KC+"                              \
  "hVLp40uMkAoHI1PHK8Mw0UgALCo07kiQpd1YEHEOLMMAxOMQM0igWcEwKcPiOP5aGVv1DEGeD0" \
  "41V0twoN6iyaCSnJjqzbkmg9D5s02YTJgmh1GAABAJF1E6GIVBYcAABIAPjesNQUzQkMCPHqaG" \
  "5aWgrnQlAyPHBnsGoyI2J1YxnYulVRiRRQPzUSSQAAAAOICIkIJxRjulBjTIqJu4hRhEeKQ9BB" \
  "4sCgoIikBdwJxmhIQEVERePaOrBbUFKsFq9Via8xoMdRixUQUMdH7kA4dmdBkmMnwIExawKglH" \
  "DM5mAcLxjNur2OQcJDU8jgezKGMoTTCSSjIcISEH/"                                  \
  "DAZSzCSRGZhkZQFzWOzHzIPI55kVeyaJyOLOjDeMB4x32EThqElRiLbu4hSagB4JPVGX106CQw" \
  "FxlezAwL1AmnI2OgJ95FTYTXUWJCHXogYjiJ9zeAQk+"                                \
  "MMzz6qVPvGfo6OkJ3T2dnUwABQIMAAAAAAACDQwAABAAAAIrNerQYWv9V/0//Xf9p/2H/ef9p/" \
  "2X/Yf9Q/1b/BEzE2jK0NDQQNcgJ14gIsT+BVgws0GAgojdE9hi06B0utk6OBEYZxNa/"        \
  "YWB1IIIaiC3o9ycQrxHPWQR2Beo57/"                                             \
  "Q8dRMQZxfcCgCgLFSCVcrkbAMF1KpCqhQAnjY0MQb5gPRhjae0oYk+"                     \
  "yA9IH9Z4vmUNKSIyKUUp2G7NVa66kjgVnUUcZ1EOBAAAIIiYWLIJMjJIQolT7uJM00SMFhFj2F" \
  "nNqfaGWu0x7MVi8SNjYScCA2AY4ksuMpMZDi6yzpho1rAWIcMx8IKBa+"                   \
  "IZaoHVE4SPIiKyR1iEDMnFqYXM41L4r2C4C2auQrtpqFOnM5JMmHkkHMeRIeF1vOYxyafjejxe" \
  "jxzDF+"                                                                     \
  "B6cEJaoEZP9HoSwUAZLvROGEM8OINxkgiqB0LJCojXQ98rMUTnbyQgCKKskRBKoYPTAI6RpU8J" \
  "dfOn9Bj92A+xhdANvOATJADQqojR7XLp+"                                          \
  "uW0OQjWYCAD4hiDETvevXUkAjpgjECjdaxVgTpi7BgQBghIvwmmQweKgROAYQAnxdyfDyZVwGw" \
  "GHRx6WToDAiaKIgB0n3G26RFiNDNjZQ4AABbyfsJZF7429M5FZh8N7ZfZo6e2YfQuMhtpGJ9mT" \
  "56PdjGbaAfz5LpdV+"                                                          \
  "gcuhHFY8dAAAAAUMYUGOJgSkSCAoGAQkH7NEkIxQUUoWgBamtnqDoQx8wwzLa4VuxtRcQwxM6i" \
  "mGqnKQgBNWW0tiGP43gquIHMiPBMTgaPsCI8ZgZqRyCZFwfDXMfMwIthOI4QjtGYuYgAwPBipc" \
  "k181o4BlCAhBIGF2aOubgG8uDBHDMz4WKGPHhxVXExgRmmiiZqE1MnGAB6Z0gaGuHOQEANIaCV" \
  "QV8tAuHgAOBFZmI3honY1a0m+v2+"                                               \
  "jzxGOAidEZ46gShAZwqN14ePmBfHCnAAAsHotmG8h8swmg5oTWjInYPwm4BSGAA0Qz8WEOK0MK" \
  "kB6L0BHLkmVidRw/"                                                           \
  "CXKQaiJhRE56mXHICVMMNHIt8dVwAAAADQYAoA9IyMjJ7oqQTSj5kFKAA+N0wtxmQPSB+"      \
  "szA1jTSF7QPgEji6TiF1WMldVeoorFzqdKN4sa+"                                    \
  "xAICQkRAIAAIZUUE6xD1SQ9LIn8SmvZ8kzWFRMVAhJmWEhSJAyL4MrQAABCCGEfYoqBwVfvvLU" \
  "rU/"                                                                        \
  "4c2L1xM7OVuzTDlVwzGqjVrEI8prXcU0y88plIvltLPrw60Py4gQ40ylhApdQi5FAHcwDkosPF" \
  "zB9RBhi2eV9xrjA6HRKoY9s4zLPO6ZcHEoCcDGvYTLDPI/"                             \
  "PbC+uHFExMNQWcxog2GKZ5jpUTZBGGcM2sX2mQCLtBWzc9M5oBKIhr2nVZDJEvSJUA+"        \
  "bQsEDzom5Y7CBDnBL20EigLuFgAMG9AkZhqAXooQNcJrRGBoQkwGOCCf0JQICKmBWByTq0iKlp" \
  "+hN3YnvaOAbIg5khYARg0RrITBhYACXbggyTaw4uJi84xkxEBhVUAAAATK6zqrqO9dNIgA+"    \
  "4ABn+NuzexMcPq+gTqae3YYsmmn248EvZQ0C6y6xWikQZGWFyJ9ZmmaCqXU/"               \
  "0xDlHOc08ToQAgQQAACQxCyoKXMYUlIiAeJhQKkm7OrF0BwRSl9Qzyq+naTefUI6H1/"        \
  "G6XhfX6qqwlu8+DWR+cCiM0qKDbrp2C04LrjmuaZEHM1dUC60NP6HujMTM9SCbkDmuHEeu+"    \
  "QETMqpjoWG7hHjqCZiQU2DmIg8qhDnIMY/"                                         \
  "rSrWjQTUvLdbUWsHpYRhQmVBVRamO7zK3TKPOlFoV1xChAZB7O8K0/"                     \
  "17piAkJSr3pA5yUM91WPSEOzTCRBYmgoSE7UhOkc+j0AGASjpYbGQLWggCQDLoB/"           \
  "EqPIhdqU3C5wu9BFhSl5AjAcPBYWnE1mQsAAADhxRjzGPI0AGACAACEOgkoEXAjXOTQg6ja2k2" \
  "ozdSZmEgougvGG6nOAqNiqqCGKkxTGXF9GFQdEgAQWU8AACxYFD9ZTwBcHAD+"              \
  "NizJxdgf4BeePL0NUzTB/EAIH3jy/"                                              \
  "K7MDEAicmDsIKnqdq7MQiw6UcSxCUFAYAAAAAYOS8EFAeHGuBwX8vRSBSUAMxGIOSZgEXEBYUp" \
  "5UkCRGU6IomKqrW8MFWPsE+"                                                    \
  "p4BKLiSo5cWxwy6i2aCIvGlVzkQZhhqre1vk9GIfwduyysMYfp6kqSHOQRAIWFrTlamUtRVOsC" \
  "i4ASxy2Xp8bxI6UeNLl0JOylIcm8KoEFTxgd0SGyjzADxwVh5p2qbG7NoLBCFj0NvaP21M25Z0" \
  "CihupMaGN0YHS6fEgnRjQmpm83Ol/"                                              \
  "jVmSZIsJTR4674wtdQXMxTPU4YYl55a1SW+"                                        \
  "nKRe3FFAlAmAhGIcmscRzzms0MoAVaoI95esuyDgBoTDDNfS8gTmABgIQQkH4R8cepiMUvv8Yx" \
  "DR+"                                                                        \
  "M3lgy6zpakJmQK0dy8SHhs3CtwLJn7gACzghHACADVAAk4JUSb0tKfjb0UobsAelFTvZ4NnTSR" \
  "NMDygtPnu/"                                                                 \
  "qsiIRAoGNNZ25q+PSqThXbDzNxIIAAAAihw8RwCQFFUtKRCjmFa3wFAiFFWIV5V4RB/"        \
  "aAb4d+jW3v1Ll3hOFnwiIRzm1rGKhPc3yaHNfBNY9j4PW4UCBz3NIMKRb/"                 \
  "Yo7Zgi3mhEMIHYwD9BaCSHPCGeE1DxXXzMxpB8z1mEzRc1bhNy7PUAIsIHwGl7vCGhk6Y7wXmG" \
  "vO4pK+lbRF1hPoXU6dtybCUH349R7h4KmaiyFcmTA3qJCNGVzAkTN2GcUle/"               \
  "mWDIKhu76tydqwxRWyU3KxDUNIu59GGQwRZDu7naQbpjp7QCTjgcxcM6zwF4E5Oq0eoRG0o6+"  \
  "3hCHJ0l8UhguOJFHBYmTcfN4aYJ10jKBL7xEZRcAIaxUQw59h3ALQQJ1wU/"                \
  "pIYPpbVNRfRiBtMpLXDDNMLDGMhsCo2hgOWsIyvao0kQAAgRgAQAPLaiUAXBzCv5fAVKXAdwUB" \
  "AGRZAl43LF0nmR8b+TdyHHXDNOpY+"                                              \
  "2MjPvg8HhPD5OZY7Oy621URi7stBMTJI6Qoh0AkAACgRGUm4oQC7aVExEUQLBAFERX47hSBQKW" \
  "OqROGjZ3VoVVU7GRyl1E27YQKri81sQoR4Yeehgy4+0R0LUBtGnOVfneARcIAs/"            \
  "bIqpLUhiHKQFpM4F0ZjIQrVgjMZLmkwEQVhFaMIWozuVYbnAauQIQXmKtKmBVApYiiqBSFL/"   \
  "NFWWAZvstNhHESPTJXWnpxhDmszIoF0OzhMNHRotGHmZkRViFoQvXSjkfR5jgqXPNU5yPBSWjt" \
  "ElhAARBjYLXHXDCEqZYhx/"                                                     \
  "BY4WCuWQmktTxgkp+6MDxGQRE1jaGRSdjOKJxwYlrOrE1EEAA9AZV0UwB+"                 \
  "SdMZDfoRAACAUfrDY7euoerjy7nMmgTkQxIIJG8Xql1h7lLtxbdXHcFpAAAAoCZPi1BjyeQAAE" \
  "DcvM4MAFxwRLKT+"                                                            \
  "VIFAB43jMmF7EMR8yeXuGFMJjL7UIifPO6qooqQMkhEMGbu6nhjkrhYRNwsgZmQIpBIAABAGYi" \
  "YBZiIIJmwkGkRES5RolwhpMUBQaBgYZnA933QqL3VcVcmdTKMmboWrGdB1er50kSGFKPaMT/"   \
  "mePHi0xWF3xwXcyQXmcAjheOaGqx0pDDw0LkYoZYZMseDyfEIrwEmF5lc5LJl1wU5dNFLCAlpT" \
  "HjoHAxhmLPCkIliCmIFmc6G0QZ71bSoKBgYmIjkDJu/"                                \
  "IJRR54TLhyRwMXrDIhj0YW51qHNBRYJDEdbIAPRhyDAMC2pAEZkAPkJPXKBwMl5yEWyF4QqXk8" \
  "JTuxwXx8XMHWeFOfpIFp36ZVZnREf7pe8jT7pIFKErIuL5806oVQGAulqAwEwAQBQFA9hKQzAA" \
  "wGQSGHjAXBfDHGGuDzk4nRKiUmplyvR2CNPZAwAAAIZh2idUVVUB4YAA3AXkmytQOFQO/"      \
  "jYsSSalkUYOFznb49swJZksjcxCeZCTuryriiyFFCECllZOnTHtXBLXxtLYhCAMAAAASAgJyYI" \
  "lS5IshGJCQiAqFIgJxZl9j/"                                                    \
  "333W7fpr05MZp24cSkeH2tSnMsFKXMW9qaikylHLmuSXU4lCgLP05DdRTO3rc7GW11BpMW1kY0" \
  "WI0IKGm0kx5qjFZjIpiivDq3YqSMulB1Ce/"                                        \
  "f4dTnLa+"                                                                   \
  "O2IKtrKH2mnnSRpi8uE6bvx0rESWgBr6HEKpYWSJNhuqSdKnDhXeh6MSWruKu6hyZM0pIGDjEL" \
  "xUMwkfCygCHDg3Vha6jhITx+UIAnnjjw+oylIFs7gYnnGElegYIR8hw5Bg+"                \
  "hDkGCCPhdlFHjUvhRwBAFvB6i3CGt80JIf/"                                        \
  "eAE8RviQdBRYacEIEQRAijEDrmjDFbopMWMVTUQzf7fUeI5iGHsD8pfkrfjOnUgAAAECvF0P7e" \
  "aABAEBk1zsCAF43bFFH88dEXg0/"                                                \
  "6oYt6oj8MQl5Nfy4a8iSSCIRDjYnzE3veOwqsYrYTG4CCQAAwCBmEEkhJQtJtBhExUVExQQCCM" \
  "XFxIgBTkxvbxo6xfAvFov/"                                                     \
  "GfBvljmtJ0T8a1m0dl0zw+"                                                     \
  "u6AjM55vVYFWEIA3NcMMPkw0FeCR1DdyIiyY95XI8p5C3kT3nlAJ4C16gRnoRkRBgPPa1hi+"   \
  "xICJTmMUPITAG1mlZ7MAwbw9VO0Pp9i66wIjSy3hGB0zDC6wx5wIvhNQwBeJAwcDF0TmNRZ6Gv" \
  "kokIwm8hmNM4Q/"                                                             \
  "VY0Vk0jozLWlAXIgwjP3aag9GR3qUG9hhtuoh82CAPhrnmEwuu6yhGhILBFYlQQp0UodRTCUAL" \
  "sQ/"                                                                        \
  "CRAtBEE2YuEWKMOBAZEAPCBm3AwBqSZcC4BeYmABMAMBYkACOxyMAAABUHLMaUgAA2BqfXgMAA" \
  "NQLHjcsSUcOI1AfPA9xw5J0TP5A7B6GHndFdRIZMqEUHLN2EIzV7ZxLLC5sinMeAUkCAAAJKUg" \
  "ykSBmgCgxUTBDhIlAnA6AhpAWEOJXiCahabB1mnjOpaZaDBUMg6lYBAXMAUPUdc3AECmyI4DMZ" \
  "F6PjbxicKPLSuf1kYQAczHD63qQXLcAVbRu4BgmeZGLgLQwDMzMHD8yoJH1HbhohKH05QjAdXE" \
  "cjFBH1Bs9o8d1zQOODEAgAhERxsBFJrklKxcDTyRjvJPAGLtbWAm8xRHNaEgYi05XJJ2nLjiBv" \
  "giHThJpY6P1o4lBLEYihlBCdQRhaHeQ1IHRGO+UHK/"                                 \
  "JNZMUZdUIdTGA6OBDwhgE6ToiBgCAUADXFJLZRegIIYQoijAitBpBAYB0mAfamTgAAMkqChSAi" \
  "hVZmAAAoCMFMlhXBQAAQKXfX1ZIgOk/hyXbVQAAfAAuAAf+NozRRI4/NiVcwNswRhM5/"       \
  "NgkF3DXrFYBRMiIyNOM3Vpg2lW6E2NqlUIXnUAEIQEAgJNUJSAFRcFgZqFATOApLVScEtKAmDh" \
  "Nu4kLKHF2l8sUdJljntIH5tPxOCSuKl7MKvh0zTFIJ4YaqsLwIONWlytkqK4B4Qay8MgE6kzDc" \
  "VwXgXmRRb2erGJHdpmQFiLadToZYXxknY4YSzNHOKWwiuPDI7kyc+V4C78TrkyOx+"          \
  "T6CpOBqTS8Xj+OzDFkFmE8jajuDNSEtGiNnsEbwOkYSMDFXAFvQPQK+ohQo2XDs8hfHcVMZG+"  \
  "cej0BjCdmMwERRMUU1CL2qFzHHA9eV9RPZ2dTAAEArQAAAAAAAINDAAAFAAAAVx6YmR9Y/2H/"  \
  "V/9m/2H/WP9Z/3VORkpMSk58cGtuam5nZ2tqCCObwnf0LmQeXJnfa+BIPtTG5UaNjfA6/"      \
  "XTYrl1Li5wcMDBMyyEBAF4AAABUOFPjubFDzrqMwarPAKjxugADIIHebgW63cx9egLIFsQMGgk" \
  "uPwwAFbhABp421M7EjP9ohbCwHc6G0piw+"                                         \
  "UcKuCR0nV41SQCASbETOXdcdVVFrlJ0c4QJJCMAAADPcA5xIzxWA8M6xnl8N9fwkEppIgEPyXI" \
  "3oSjtWYZYTHtnDH/Z7mKZTNdfNJ74Fp3eFECMtjGQhfnA9UYpJ/"                        \
  "1YvXJYOTJjany7DRgsABTFaIsrEaAurw8LkbAxmeNrAQQr05IYJkEtMEgE9W+"              \
  "PA3LlOF7HhKtFKadthaM1K5xauaZ9l2vmygFMF1ZJqI6V4pUBIDOBmyw6DXHrSMHFUMWvchozI" \
  "Gwb0TA6EO/yEQB0PhIsIBkT/"                                                   \
  "RZH60dad4vrPXQTKg9UoU0grXHkSLlIuGTg8ehSXBchQwkIM3VasAXXhY8n0g36Ic8XYiWlQQt" \
  "y6B0tXAROgEgwWAHzCgAAAPezppotW0BBAOCrZgzA9ZIAbrfLxAToYGdTAAAwJlYqgErGhYwDF" \
  "BfYYEPg7h84CgC+"                                                            \
  "BkxRJo1LIaeFgiLTBkxFJxsW5LQQKPKqSkhAwCJkbLnspE5VuyqLa7pJKULKAQAAAC+"        \
  "VM14YjpC4cagMhwhyKWMIadCdl4iKeFaQ8umy6RmFTzy9AlpMBGJer+"                    \
  "FT8iBH5nqQyUG2wAjvysAMw1y5VtnmhHS6Iul8+"                                    \
  "KkzguhJqEXqgoS8siog00U4CTajcR2vOTIhZCA9jeS0gY5a4Zr5cF2Pa5iAwadjOK4EJtfNnlw" \
  "gDE7zGAGAKc2R1zFpZAYgw1Dohu8LhgvVmdqchRXCFiw6zuI9hYwa0n8F1dGaMeAawS6RgGF06" \
  "b2OOkicjlG9t4KZHADwuI5T8ZihdnE8husDr3nwSUgmkE0oTLIEc/"                      \
  "EYZh9t0hS9h8uEpMAf6MIv/X/"                                                  \
  "RFyLUMrp4HJEHAG8R8B0BAABxCQBw8TB7JwdsAICKjT0CAKLW6Cx4QDLX9fp0DfnwVQDeNqzFx" \
  "+YHKBeePLcNWzFJ+QPKQk42xndRAYJMQSdlrrmqXSplrsvCLKUAAAAAC7COyxK+"            \
  "gDiOLYwwEihIqTcioqJiLooSnzad5t+"                                            \
  "XI0uovdXHMqONWu1trehoTigttn5GYxgHG8NkOF7XrY0L9PqG8IxsjhZYcNZ2LQ0LBYlEXYwl4" \
  "VWCmQCEa4WbVrf5ckEGxLIEdx5WXWHh0h7AAEdZEniGHUFCRgpmTRgSYXSOC401a4Qh87ZYdDo" \
  "VkcJurIvWXwpjFF2RnKYI6iwwE5UUojSIgvCdauUWic5Jik642t86i2NiZAeZEMbo9DqjFwgc3" \
  "dI1QlOla7G0pQzDkLlLrGqyF0BmjlzJ8XSMs77qyLUbDQ95sTx2hEVzhs7PbdbWAT2rl4TFaQE" \
  "I+"                                                                         \
  "D7KBRgTjiQAMLBEOZHA5R4JAELo6Jn16Rh7Ygwshn5kljY5WKeDdAGAIQBuAlQtDFxcAIxFvZ4" \
  "6HQSeNozOxc1Dk9OFJw9rw+"                                                    \
  "RdhHkYclkNPBnDq45JAGDkEHO77q6E45qlROYgAACAd7s4eePywOcolC/"                  \
  "CqoxMsaiwAiIlUUI8iJP+svEx+2QbbYpv9TO1J7H1a6oxTcxBWgfTROPK5IqK8+"            \
  "JhfYfXcWWOuRXMMYiwBWZnV4UXB3PClYAWAbiL+"                                    \
  "fAQ4UjpD1cJok3mLICBex2oogaEeRjDHLW4YbrlB5mZYQaLg7nWTulBJnM0huPIMR/"         \
  "mEmVyRnVK4iPTIAvdTkcuh/2BCAaHr9EOEIuk2NVw7iAl/bqH56EwAkMkSIdiBdAZvUt/"      \
  "ddH6ZCx6Qr1BEyARBNdw0azorHiMIbngNbOAdrLwOhamZSgiIrQ3Y5i0sPQUNtb5FifoEdtmD6" \
  "RbgBsb6O+VlBWA0CNsBtuLG4V7IDw4UhUelkBk94g+/"                                \
  "WKK6YV11wYAoDduA4B7z1+"                                                     \
  "6Lr2XymIDAACQYEnIPgcsKSbSAj6YKwjQHLBEn7INhBd4fleUGQAyxJHL5NyZ0pUUOyeW6CZHA" \
  "gAAgMcIhByNclkurFTAJ6Ll4uoVifIQgAgrxMqn+Ip0ZFOu+mBrGr5lMid90mIxp472/"       \
  "h0nd3h8F65wXOGlXkxIaVWNMDNXKcF+"                                            \
  "XWbyxBk1dBIPP6qBiMpUAyk6MXBNBqgqicXujEyd1JGBMa6QIBSNwMwr18DcXoMrWenWtU9zHc" \
  "Pjw1yZBA7ChMUXOmkEwh9q0TgRBH2HUggSegw4QUI3ABdsJl62hemlSmOaFWRIAgu3MMEuD3H0" \
  "e3RHJA4JAy2SxQaaTjfDgFxTUa5cGWDABtb1HhdBq9pox5U1RTkOvXr73dZtaG01Ebq6Tn3EhQ" \
  "sAgU/"                                                                      \
  "oNpFRIgYDVYtOrlF1qXhqryu5JjoinVEAzovr0NEmAmmijwwTu7EjrkABJFseTe0sAIC5XjcDA" \
  "B439CWWNo2azpJW55jpHokbuhpj7YdOX1bnmOmVuavKIgmZskwZx4plgyWamrXzBBljFucSRaQ" \
  "nAgEAAMDNwYGJSDxRWeEpLvBShHKoaGgrMYREkqgIXGLihBBHjIWevu8pJAQEAChnMfPpE8x1M" \
  "Giva5JHjszM9YJrCWbgel1jcW906QAgsipqWic7DN82mBrhoCM6AIbAkPBf6IgFn5zeMeNJWDx" \
  "8KEGoy5HBSoTj4WyAmmIYgqoxNSwRjA4GufTGBUd6KwcFnATQ1y4XgEOAEmix6dkdRq9bahQaI" \
  "N1+iK2FyY1BI+"                                                              \
  "sIjbZGR1zUQnK6vLHIIEHTNddrhms0TqZnCEkNSBjXzKwCciCFRabeI9qEQRQoqAc1FkAcp3A6" \
  "S1QoAGDryQi0GOjDGwCWhRcUABAGLhv4IIR+"                                       \
  "v98HAMhIAPDppHj2hkWVUlFR2Quwt4tuJIMDuMAHFjYkhSzy/L8ZsuVnkSupZMresCEhRDEM/"  \
  "83E+"                                                                       \
  "rPKlVQyUfdHWVGkQIgyiCJFWYQKZWQxNdVNvQGScooeYtPZmEpKYhIAAHCFggYx8hmGFTMyw6X" \
  "kLgFRCEILqUX4oJUQBrvAvUjJfibTT6cOHNjFTHG87DxduU7Vh1wXTK7hOK7VXhOurRyPXMOkX" \
  "HD8VhbhdxsEhiGS8OKgRfGlrbZqqHavXmdA4FQYQgIwL1AO1J+"                         \
  "p9k6nmV6iJngxIQxLwwDMABxzw2++ASgxOXL8MvPI9zEAkMkcx+"                        \
  "MgE8hkAikBLf7alVlCnYIwbGfGzNqCVVuZbUbbzCE969R0OpDQkBYIQQ+"                  \
  "GOGKZBm3P5IkRZYC5kDiu65dJEtJipop3Fi9mhhkSgwpHDsAKOQpwg3FnuhicFPXBW8vpSlEFW" \
  "AEAd6w8ecSZhJ6bwBva91t3Avs1S7b8yQ8yYqiiowpTDXokAFBEALgsFJz7CeCaU3JRPVysbc4" \
  "KAPABPAV3W0MRxvdyBAX/NBUV3OUMRRi/"                                          \
  "nyNEch+cSQ2VpTKTNyM0IErFMTvMa4a5MpPhQS5XZL1D6KOyvLBW74Q+/"                  \
  "LZQzerw1OCbEgBoYNcAlAULDSuNVA8UfG7aMmChYqWR+"                               \
  "gUFvxvTmmpGGXhTFQAawExCsAe1iDDHBw5GF1LvNJsxFm1CGgYLtMU4GJj5ziFCfRjzAmQBdxi" \
  "QcF6kggXbPLaC5Q2wuD6kggXbNPk8kQApvRGEYqj3IW7UlXAthj2gJnE5JPiQ60gy4RHM1ZBeE" \
  "I7wIsIAnjgESCiQYNgZpAW9nBhHsr5Ach+"                                         \
  "YWgt6ODGOZP6A5H5QfwF4cwRTlAHf6yIWknkMlIDhADBRk/"                            \
  "k0pFCnwxsjESZ3GmFcMnA9Ul8YE+"                                               \
  "JlBAHyP6UWANQBCyM4JH8kgZHkfsg6YGEEt8SHzERyL6ijogqQ5j1TUFErK+"               \
  "SzxPUKcwtIRPgdgWuG11w+"                                                     \
  "OtTpLVDDZF4zx6knBlUAhwDICmQALCKB08VwOgaSu6apRiR0jpu3QyC5zziNs7JaCAqQZFQwMN" \
  "c5uY3QLbMSrgmNzxINUxl7OyYslrwoYTSW5hsgvtFBQqEPnf1YBIBj6z8BXDpDN7Ff+"        \
  "Wx9MUF2jU5VgT/"                                                             \
  "1w1Es0H+1J1qEFCsX5TGX0IZvTtN9S5llTiNyVKec5rJmvh6CYjGTi9Bn4tLhaREGlmcL5nBu+" \
  "KDZnq2Kiol8RH9qfpfQf6ZSVK0dP/e/"                                            \
  "nvvUaGuZNm3KVN++ZbvTOpOdjzawOi+O9yoXAHQ+0RXo8vzCWFyz9/lEVuAzf2EuPvO/"       \
  "sjx0WR0hcqc5I4MaznYaOVWEsijZZpWBique7CZhMbGQZfEyWVyG0LMCYp4Oza4KUjZViMiSQg" \
  "/JWxerNC3JT/767BP/xVSfXYYZZ5wwxSqfPgto4LprbSdcLtEVXI/"                      \
  "fo3THu1TCFeTTKaXN2VbNrK4qg85GO2bFWkWILNyZbK/"                               \
  "KP1NAdz0CADFN8i4ltJAISgLi0X9a2ynottztoFPoZvWfneL3HKV90HsypJK7Z4GTo+"        \
  "rlc5fjyjLaOUypO8x83scuACw2qQ7qtd7EAgV+"                                     \
  "5BJXoPs91L7p3TgWRSQRmxcH564OZydzCfU0SV57KRj4ToRDUoFkPXZKTjVV3okKiaruMtXOiJ" \
  "vA0/zjx//HqlnfY8n74+l7HveURKJzFxWIUOel4rq+e5dTPPUjM/"                       \
  "23WFUCjErBCfplfdjiwjcqhRcQp/"                                               \
  "qYYjPcVVZEYmJ2CVlVIYnssMHUPUuHciRXDLCaCJMScvjseLhRLq8nNuOopQoxmYiKv4i4lWhP" \
  "UcrfRaso7b1OeIiPC09dSCl4giLiB5UKKWZX6D2OAPgkAGQ+"                           \
  "qQOs25VQbOBdQtEVaCs90Di9V11URiLaIeY5Y8x+"                                   \
  "s01mCsGpqjlRM0w5kbtfAbg2X7Y5ooTqyNL1iX234M3noeyJB8rDzTbe/"                  \
  "NOZnDUSdBctE3OXWHOV0Hru/"                                                   \
  "z8zXbkYTn+tVnNvilOx1wIAjE7FAfSov5nimu1oVZtB7Pk3U7xm/"                       \
  "4zKyoR2Zhumjfs8c1oUMI8ngKhITnHK41EyJeLu3qyMogICupCAL8aiNMUQp0VFiFBUPJj4EzN" \
  "25n8O7Wc6Go4cOdiN+LLFwUwc+"                                                 \
  "AAVvvxKAJxS4QD9dn1JxTFeKhXPoF7uDxSL2dtlecgRKUQVisiLrMpA6ggbRs0jjnKEfgOgpdr" \
  "GJEmCSIha0HPVlKMFk4QiE4JF1+bDDBGaRT0YxK13A0tkcaGKi4iAYFT+/"                 \
  "W2YfgarKWpoBQCMUsmKtB5iEFZcwyutkg2cSxwhWlzjv1ElEmfMLMrRNnpJm5k5p0iVFcoR4QY" \
  "AaqZoCY7B2Ht3D9GSj5UqYu5SuApSXjaqRFGWFWVX7g1JNJIsdMVGBnXsrEM17v79bbU6mtJ+"  \
  "1MbHl20AAFRavZMB3cRI3ALd8C+"                                                \
  "tnmHEN3GkbvHDtpvihMRpzsEZGciKmJYo5h63sigOZkVZBsqSekW2KwFcl6qEF+"            \
  "YzIkKKQctyeZlBtNogIMHzUTQ571WEF0+a8GJSCLgRqQb+"                             \
  "Ga5LgvTOmLssAABPZ2dTAADA1wAAAAAAAINDAAAGAAAAssrB0Bhy/5H/Yv9Z/0j/Wf92/0r/"   \
  "Vv9N/17/"                                                                   \
  "VP9sOrUDVEK81OJAtxnVTcDvIWrbbP+qRVGJOCSJg2sRZVYUgzhr8q+"                    \
  "OySJtSwRgYVvWvMClNgVxqmmBq7XQR4pyuSiRllAhTU/"                               \
  "J80pq59H9GYf2dEFIWTNjavbxba7fe+OzU2w2vu0F347gu3zb2vq6AQBaJx0+LCWy/"         \
  "w6fDJKL0EZwnXRoeIwP/"                                                       \
  "z18GjtrPSVoJb4DAJBZWQaiMiPZj7TIXiTY8nimN0md5gjdTTmENAAJAAAAoAIUURwBLHQ+"    \
  "h8sTCIWiPLNMeQmDxdwkmdC+CLzUAdMyBAIPBUlZIkwN7fnb+"                          \
  "9uSwtRWkmuyyocmOYtBEel1HmpeTJ1EVLFOb0lEnO0AXMdjMseLD1Mp8wDCL2uEL0vATRNAZyp" \
  "a49olwNrQqAJwT3iAkFusfXgdw8VZCVX8WMGwTjUUFEPpfgfmOncd+"                     \
  "rHhdRSxWu3EBExD0NS2mRys5YNvKavYqwyfMhfAhNCeup1HCj7GrEZVA7mODVjh+BT+"        \
  "9PvuptHu3jSigQFX1zBEmbaGaDb2JQaCVRhGwwoyevNdOw9O15QrnybH5LqurGVP20xayn4QQB" \
  "FRLNMQF1QHHcxIBM6wdmFYKUG3AQBwGjQAAAOnZf1Ub/"                               \
  "e9v37rOD2YuEfcx5PCbDSLZCYAAKCX25caq19ul9qapX61+"                            \
  "lIDABAZSVxfXy8QCQBYc3MAHjkFVsyd/TfmSw7VR2o/O0v5UYpsnz37D/Mhh+wjsZ/"         \
  "V4v2goqyEoiJRoSzJogx4HDqDIpEqE6EqJWUi14uAo+4E4GnIM6qbRCCmFwAAAABEA+"        \
  "qwXTLOUhBj28gygOcTCBhaGosRLslN8EpBjMajIMEsUkAQkyAGBEIBE1AQJ2AKYAGLEBqgKIqi" \
  "mTCID0NNSEaLLkYDnQvEN18VGb45rkw4rqOnYaV6GJ2YACioYLGzmqZFrQ7FFAciIOiIIzQAAF" \
  "4XqrYWe4vVBsVQUUGGOSbXcX14JZmdaYqKKIiCoa1NBDF0zSOW7AEDTIwyGBNWY8ImffuJWy/"  \
  "n9DB6AMBHPXz4HIbqDEUEtccwTauqOKLOMMbD5ZrjmhnC9ciHmWlnscFiFSyY2IsDPPLpyHUw4" \
  "TpYALQIILSubgSg3+/3+/1+v98Pow83Ajh3lCBkrhyPYzJCnQIAAMgovA6uh9EJAABeKZVWTb/" \
  "60V7qPRw9u5P3C16lVFo1/"                                                     \
  "epHe+"                                                                      \
  "nu4ejZnbxfsP4ga4RCkllZkYEAOTnnefRqj3JmH9iZ6QAhtWtTGgAAAAAAwhLwGSFHCB6XRMWU" \
  "R1nKslxCGD5DweeAwIEwXKCZoigIBdNuXW+"                                        \
  "u6XmVQBqiHoQyTBsxHNkLpmlrZ6uG48Prxeu6Zo7jw4cPQ/"                            \
  "hDnTowOvIY5pjhOq7rypEF4vTGQlhZ3fSY2+"                                       \
  "AgDOHWUYPHMMxFEsRQFDUNMVA7Hz7l4nhdwPAgwKFaUbW3qGCxcb1CZsJM8kpAaJjj9bgCwwBR" \
  "mPCFMYS4rlcmBwPMkWFyXJnMdXDMCz5NJvMhMGGA2ohpOCbqmDghiE8PAAAAAAAAYq9qMbFxYI" \
  "ppZ2M6hmNitXfwIddxvSbD9WEAo402Vgdq65jjamPrUAXEsLERAPV6TcJIAOBooXUEAADAsb5P" \
  "ql+mBQAAAN4J9ZZPu/wP45f1bp5ZsmuFdUK96tMu/"                                  \
  "0P7Zb2bZ5bsWmEPRHV1EDUkqbKiSFFUhCiCuV06Xs1G9VyLiZQHAAAAACCClHTIhfTAEMK3JKn" \
  "VdmIjsNo6ZtoYYmOjIlgdM0yrKXaYNqjVDodWPB6QA8K8rpnJQcJ1zeuagzkWqKckjAW9g+"    \
  "s14fUaOI4HUEYXITpqIkAM1Tu9jut4TciVMITJ8HodCWQ4YfQADOANoUR/"                 \
  "A6NjXnPk9SKBgymYKphqWGzAYudxQK6LGQAA3kSYCJcdDm0xTNMiiAiqqoIhYie2IoYQfkcA4C" \
  "lghIT+"                                                                     \
  "Bpe3CDXtBNSRaasI9qbjmjm4Ei6OeRFM03DQo4cAiLFIunWAd11M5hWuOQ4GwhUgD8jBNXNdV3" \
  "jNwQDAmEBHABgTEWBCWgCgowMAAACu49OHa7GxcYQDHAQoAP74JDM9PfvDL92dfFUybqQ9Osmw" \
  "p2V/"                                                                       \
  "+KW7+"                                                                      \
  "apk3Ih7oIaKDBUQUmRZGRlRU0QRe1pQ3VN5Q1SKidQKkXuiGAAAAAAIAsFdEIKLZMmAkwlyuWH" \
  "BXaYIxWJioCAUsIhLfKqKWkVMwKq2Zntcx8wxuRVpwpV3EW/"                           \
  "RRyZeM2FWCsOEa3g9ZmbIMLQY5qwMc1zr0sFAGCMMTGskbXXMFAO1mmJgimGu5HYFwJLEawIzE" \
  "NapAwBQFsf1Vjzy3QFzHZk7BTENW7W0VRxa1Cr2zGCrPVpMAQDAIGH1DomxGKG2aoM64TDT12C" \
  "L4rolxwGE0UN39IchwOvxOraYx+"                                                \
  "t4bwBdhE5ADTUxHNjZ22LYuD7NZF6v1e31zcx6zrEAKfTQU5eHAcLoXQwAANBiX0heAwAAoxl9" \
  "XQLY2gCw6koAAAAAwJi5JflldKc4cDAPu7NKAQAAANB9/7YUAAAA3vjE1hG/"               \
  "eISH7tv5rFmwJWx8UnWPXj186Hb2SbcVu6ZUSwQ11CIzK5AZqMgQaTcwymUaDxMWgKQ8nxgowo" \
  "TvGRllZlE2ByJDep1kiMCIE+"                                                   \
  "UBAAAAgIDwJjjo5aaInToqytXNJkNBGrxRElEWdZgihGYwiJBLAiKeyfS0hGkuioJQhMoSuMzr" \
  "ifT3D1OQMmGwWLh5ywBRCMhQ8o93apqvqaryMorpq5x0LBQTAAEph2La2hoGlmn2FityDMLrGO" \
  "1k2HS4PjSsrjAd6pDRJdE1kmsZxyOv445hcjDHcb1FHbp5aFF/"                         \
  "dXoE86GG6AwGkxNWEHOQAIxqxY/AcAxnFJmGJ1YBFRcA7xhC/"                          \
  "zvtjPnm7M3up1AAMMTMAHIwn5W15st2SgAAABBHAABDVEAEI6aaXWD0AYQcpaGbD1/"         \
  "DiJwhDDNMAgAoSgC6Edx44wDgdcETADRHRn+/9/ern6+ur+1+2d2c9XF1M/y93QAAAABz5/"    \
  "ufBgAAAB7pNMoRs33saBnTJ2D0uEU6jXLEbB87Wsb0CRg9btdEVZFU1ZRRkYQkKjJkcBKGoBBZ" \
  "GYkIVLmatjWrg/II0wm7yQghJiWSBAAAABGzZJAgIkjJLOEKWVDK4/"                     \
  "AJhyEMiIBQzKJMWCBOUQxJLCQJAksWzJQIREDTIjSIOAhhATgIPCYzx2WROHU0DBGOBI6EeXDM" \
  "os5pMQyj8SAW9RHUMOLINUNgMpPXI59myDDAFfDEIqN3MZhqbzHEECVwRwQAAOSaDzyOJHMk4f" \
  "jAY2axsTPV1lAbAYAf+"                                                        \
  "k3sDkTFztRSiwmmIWAegetg6XowdKMwEQEAiqiomFaHtjaMkWgoBYC1NYDMdQxvCiZWB7Zib2J" \
  "abEdz5HVdHPPiAbwMFgyIk+giEwDAWPSOdMDoBwCgG1aLAKD3/"                         \
  "S0AsGO+VNLeqaW8tpS3+UVmUQAu3sjU/"                                           \
  "B47Pgb1zr4gS7ISNzI1v8eOj0F9Zl+"                                             \
  "QA1npfEjVZYYaa0rVRNRQGWrImlA4ZjEcgHY9LcA4EAkwZjAbgKO7qkiEGCWRAAAAAAvBEJJYN" \
  "CIpWBKxZLBkKSUTxFmEAk0LAEdKEzEhBSZCmmJCiwsYDEIAZjBFgWkBTZiIQxxCBpimCBgOM2j" \
  "QBG6SLpaiysW0izAYAAC5wkCGB1y5BnIM5FNmCF0k6Bn0kYBPB5kjgWuYORKO12hXCAfAvBhIg" \
  "CnUYicihokCqgKTSRIyk9cUTId2YkFSAwAAwGKk0vGFVPi8tTC54HhNcj1eTwVkwIi0LkXKBHX" \
  "gARgOrgw8hmMekwECQ2YE1OkkobwNQBYRJnlri2G1SA2AEuCMFBHhtOCQGFAAiKOBiTjV7/"    \
  "e7fToicLYBgA8/I0JEbe2tpioigHcsJAMsiZ8AgAuwcgG2EoACIAP+6CzKkpRfTI97tbNDya/"  \
  "RmZQ5Kb+YHvdqZ4ew/"                                                         \
  "zWFWguhfSWqoCpTlAWE3diQ2hvp0wIkujNXNRLCeKIYAAAAAMCBv2Zuo0sBJbg8DkTw+"       \
  "MIwCAPKZUDoEgUWEfeCkoAQFJgmTEGUov0iCe1pGvYq9oaihtqiYo4DrjDhFcIV4IKZV2CuMEw" \
  "yTBheDFNs7VUEBdRUwFTAHMGBaSthCCwAXucxF9fxIddjvjsOVqc6EPc6mGLYMgmoAAiYgfC6y" \
  "DHXAcOiFsfUsJiqVjF9yBwEAACApje6qBEmAvI7gAkvXjmOxzsdeQKAAQBUUNbBysytJu3F1oh" \
  "FdRXJMVcAAGC7PGFxhSQkBPJYgAKeXkcIMqqNlemmODCwEdsIO7BcFggFgw4oIuzQE4QadIjBy" \
  "mC0xnhEg5BQBVsDh6JWC6oC5pYeM69hmNeBEQAAAB4JrdaYSBfbx+ys5DS/"                \
  "SGix57TsYPvYPTnN75qozqCqOlNZERTVJUqIPCezIGsMImlqDbVXjk54G8BQMRM3y4BiAAAAAJ" \
  "BgiuzGvaSw2COcUPAsKgwQtVLFCAXMjE5kisGCZIk4oKgqJGEATADaK+"                   \
  "JGBBRFi1B0EFqu6zGZD5yK7w5uMW46gNdkroOZTDjINWPlOCJ8jJ7ICHT17Ywa4pepioEgZozV" \
  "BLVrVvUp6wihFEbP0BZBDaymyjQRIY4Wp6b5bo4ClmwhM8OlruXD5FvIACRHyKeT3e6EEYKKho" \
  "TgnO3b67hOKQIAAPir3b0QhhICAL2E4x1zzGxcR0ZmBTL9qCxGrjFhjGgCiH2YOhgDKmJ1YtJy" \
  "8xUAANWeUkgZSecbRxsRUxXt4/"                                                 \
  "QEAOR4cVzHpw8fzDoAAADb26LUAxeXWn2M4NKBCQDoUMcAINMAGPK2yQOAifAxxggAAAA+"     \
  "KZ3VGs8Opl93kpyTI2xSuqg1nl9MnzetLicOuR+KTFRDGWR1VZCgOI+"                    \
  "IyqoIMjIydNfoqBUj7I2ORdytWZAAAAAAwGcIFVIOy7IChku5BIQGoDEORyB5hHg+"          \
  "SUkMFpICMXcoMEXTghIlQiAUo4itMX/FEAJfOFaddCzcisxAVuDI5C/"                    \
  "BAaNwbWSGqmY1Wuqa6qt1FdYsDuaCmYGZFZiZH9fr0GEiFBWq4Sm2NmJVe9c1lSEBFLg52NV2T" \
  "DBO43HCj2fCdUyAE2rRWyCAs4Au9QQnccwQqfhskEkGAAC8ziIQui0wGgJE2NKnM8J13MkY5nx" \
  "qwUfou6OV0d0LMDwVwLTgyyL+Ej+GKz80AAAAoBgiiJNi6+"                            \
  "xj8W2q1alYxUCsgqc9uAg10OkoJT6yU6oYCBAAYHRFEAAAdAHQIgEUYbVPMI/"              \
  "NmPmU6jRQDKvLdYsxZgD+"                                                      \
  "2FxCl2VHjIAbORGNzjnjs7IRze9PTPwQEyALiQoUlQGC2M8D6B4a2gDjkw8gOmOPvoGFmN5NSq" \
  "wAZwIAAAAARqNG0ysYsTgPzUCEQgGPbxDyNMKwoAJQCIViREDcKiihCO3uEyIAGJRKChwPdciT" \
  "pAyT6ODP5QiJiyJgGrSQVUsByq8mQcmUBD1LHlFhQrpERCgFAACgKRYpwNNkVKyGgGFYZTINPA" \
  "7y4rThFo4ZOKTMgxkVJwwTmEy8gKIrjtdR1fV7UYEUARIDI0fx7TONAC3kxOjfGM6YmExpYh+"  \
  "EtSpleWO+1p6ziSjXSooCmAZA/"                                                 \
  "okWJjoAAAAAjsdJSrFPZ2dTAAHABwEAAAAAAINDAAAHAAAASEashxh0/2H/Tf9Q/1z/a/9J/"   \
  "0P/Uf9f/1v/T/"                                                              \
  "9ThxSNkAfKZZHcwvT9bk446gQtnFQwGkw88zQxMEa6wgUAkqSiKb0lYqgtKiLTxMTADHOsbcED" \
  "Zm1ioq8n617fHM16LwAAGP0g1wOAGQAAAIBgyvQuAVIIB8Cnu8LjCWrwOJgd1o8nEhcDDFVkEm" \
  "Kq86YqAF7ZPEBJ42J7xIdyju+ubB4gpYuL6ZUfSrWo+"                                \
  "4esKqACKgURiahItg1YTZ3oE9W51Bx6RJw2sAAAAAAAh4LH03mEcIUcygFEWZbH53A5ABEh0qS" \
  "IuCPOvgrZEaGFspgYAAAA8apAKteLn8K1JdekzFHpUjEH8HoG8EqmugZMOSBPYoTVU4suGL2ip" \
  "YZOnSLCKJigBghq4LU5Mc2XyVgEBoYJxFL1LmksY7X1I6CYFsMYVG0NW8Pega4AACMRJkZG6KA" \
  "zYbN1aDvZDk9YDRMQJDKBcxkAAABeBJiTtSISAuEL3V2/caEUAD3l/"                     \
  "cT1UdBtEd47CaATYAAA5l8C3vtLABCAOegkFk9j6hlTdRQagUCMdE2NlwJDRyUrsY6VUHkxOWW" \
  "UhXnGcZ1uNPgARFboAyz0Xm8AEUyIY6K5L2MLIawsNgACDIT5kTIdGGJYLU5FRI2c7lWkCSTXh" \
  "ZfVygAAAAAe6dxyPkcfmstFTnVHOrecz9GL5mElJ3s/"                                \
  "kEUNkIGKskpQBEUlpuoa6ctkB+"                                                 \
  "iJpjI24ngAYQEAAAAAIGDBYSgFjxIenwFLkJKEFFKyEmDhiEpApsXZAcUEDAbNgJQI3URYnAaH" \
  "VAaDmQEAAKyObK1iUWeLTF92Dm38+"                                              \
  "BTCiyuP42LIMQ8CxzFzXMwTRh8B43Q5NIQ8eAWFwMzj4G2Ti8ZFcgAEJhetHNcZCRYI9QDUtFi" \
  "nU7EiqhgCwKm3aABex8yDYzL7oYU4RkCNMhGxYrEaVos9FQGYAQCA4WR0qAfgdDoB6FmMw4gIZ" \
  "yQLFdCFYdQDGP1+H0YYQQwdpqmK2IGlTAUwFzfkYODIhw/HNddjVWCxlDDXfeBI/"           \
  "VySEdFdiNTCAJSg6qC44iIrRUYLeidggE6tgQgAEAgXKCqLYm/"                         \
  "YCDbiC1UrkEshOoDBAB7ZvIlUHHNQXOVBqCSyeRUlS140a3sRGvlAZk0yRAnVWVGKjAxRGUzdS" \
  "Xmhh92Qk3iciCQAAADAY20asg7iCcEXgFCGAOpMEJBSMZtMMcUik45mOjVLZpwwcSFLOR6PB5D" \
  "hjluXmVVDXg9mDnjADBNpgFl4XFytJEGCT8fkejw+"                                  \
  "JaNkARjcrYwzDoRHgk0Yh0RHKWW0ajhjznLYjSUxDK/"                                \
  "Md8cFM0MXKMZBXYdrsGEGRFJXrfBtHac4g2oqhNWgTPAUAABOoSKElcjjIqkQADrgzYrqMop5Y" \
  "Wcni7RSJtA6qrWegui9kxuENayEOC42dzyLXDABFtPO1NJABa37q0EHjB+yz5cgQ/"          \
  "QIT4ALk0ObCELMcSJEXRGM6/ZtpQi9AOPujcpbFJfpDRH/"                             \
  "TgwthqkIhpMtGMZQUIlGULX6rZiuB/"                                             \
  "B6DADAIwBpEgCSVQgA3mi8+"                                                    \
  "5gyE3Tt01gRGbNovLqUliao7dMwvEGNVYlItLlhmdt1UT2uMFGReNyARQjKAQAAAMQQ8GnrgM5" \
  "huDEOGK4nLWBIuMItWURSVFZCEUI4DQBUXybOjpw6tBF7U896yhwn2j2K0c/"               \
  "EgAVcKxNWMI8Z3LqejHpVW1Eb5kjIA1NpqN7orXSMevC6HpNVSNFhAkmOMDnmw2sq/"         \
  "UVhZipwcYDSb2AIQtRV7rgNoFy3SCwwA6ZDahcFYXG347lwE/"                          \
  "NVTKjELdYuy5eJm3qY4l8FBQH4JFyAGYWuKFcxn3INwMw1jBOGATpa1nunK8q03an3RC90jib0" \
  "w40xMEZYUetcOt3odtA6OtIxwIXEzPyZogjSHCN2uJztxPSq73YIGD0cIjJhCG2ioxVogIUJjd" \
  "a0ACgkxuyQamJrb7EROxsdVbUs9UwGyVu81z48Kq00ACCyiwAGQRgALGu8AwBeB+"           \
  "w0ZdKMjF9KFMjobjjSlMWkVHwqYRG+"                                             \
  "oagRRJK5xqaZ3l3eVElV6CyZESIkAAAAaDTKHTwifEIjQpuFoBBjMRFSEhWyoNz3CkWsM6Tt1A" \
  "krFnvfzn7sygxqu08G9nYWBQtuLSEwxwXXaya6/RhaA/"                               \
  "NiEvJ6VMqR6zoyrDbXU8UM2jHXY2vSp8w8HkcZWbTHTCYH/CZbYyBXjqpmwJbGhCGsXDPXZ/"   \
  "geTrtyUIbRJq2maeN/"                                                         \
  "YnoDtYjFWuJARAXTAiscRzIzM6mUokaLBpWDa1QSA2zZdBksXOgD8CBBjihLH3ywlp4YFqqHd1" \
  "owTlDjlIMArNJpSxgLKzw1Xo+"                                                  \
  "B40U2ZqOcUSIKymQZqsdydKjwiaw40pmcRVqGTWEFlcIcGdYbQy/"                       \
  "I3e4JY7MvDI1R2IuCTRVAt2uUAWLLjEhdpgurtZEItvq6sXWj03snCRNW2g/"               \
  "NADYxQIRnpQAp2w8sFGEJ2WP0SjsJAN43nH1J4wAW8NA3nH1JcQCLAjwcmQuCgKkyttcuKZeoi" \
  "SASUiMIAAAAZDERUQoolxNikjQtzhCnQIEIBJRoOaGIf3vTfgYnJqzTWWY6zUSsjYOvKuD1sXi" \
  "Mqslr0rhIOK7jRlgkVGcYsTos+LAwC+"                                            \
  "qMAByG9kcbIojamYKhIipOY3yEt3gAxzETJvmUueY1AwDXNeSYydGCa5hDy+"               \
  "uqvSlKG6jYmIYVcwSO48U8hmSYyZGRHhdk4y2ZYCJRIsfoaYQHAyMeiAQ9nKsI8oY4kxOIEILQ" \
  "lVBQY0BAAoD2m4uIbsWRIxc8gWu0myBaGsUowfXXk9BX+"                              \
  "4kLKYIBTgM4hYGO6p2WDgEkxlCnjkSve+O4tAkAABL/"                                \
  "7dSQZsbc+VZbCbmdQasaeBENAOj3GRMZALlKJByPAADUxM5qihoKQGInJgBgbHkCKAB+"       \
  "N2wpBh7I6TIXTuZu2FIM4oEcLnOBZI7MYJGnNDFVjZWuVFwkZopbDBIAAEBETFzoQVOgaBYRCI" \
  "WUqDglIhCIiFIUEaXERYmIOrCooPZWbB1Xi+t6cOV6HC8CIS9mQEyhbhzjPWOooTC5Ji+"      \
  "YmUxYh4aCBgzRGwpWmaoAouDPdxlEGEON+e4gmXBdjFoEQgIkTl1yhVpFckb2CEl0Ou/"       \
  "IHOERyAwh1wWZDwckTKQEIKz0pgQAAhMbGQowUgZiBkm+dz0ACaxoLwLBIfVO/"             \
  "fUyIlhWARrJjayiTyIkXIDOM8K4/"                                               \
  "GFyE1ZgVqblQ11wSfg4M55AR51jYoTpcXIMUb91xI7QoQdMJPrHBGijj3C08L3YETVl0OeA/"   \
  "F0BAGAYHAEAsxkwjwtmZgmAFo/"                                                 \
  "rMdasMTEBZ0xMoOi9gMjhpwBIhfUCnjccaw1yguphjce84VhrkBNUD2s87WbS2rmqq9vJ2BWHZ" \
  "cYNAgAAAECcmaJEaZpFxIlQTCwOsbU4YWOxmo7ZGAaO4RA7MR1XHKhptcWwE1OcUDHFVDFU8Ch" \
  "cXMSYTISxqEdEZFuFhLxORS4UKnBhdFFQPYWbkvl0XJnM5DrgMTArTLgCB7wyAJMJCwUiNtGFA" \
  "RwcBObT8ZjAXKBdOebT43Vdr8eHTy9gyAUXGeb1AgLXxKHTh791tTphShMRY6VnND8VJ6I2gWm" \
  "E5vXS2IRGRA7G6Pplchz9iRiatbSjQw/"                                           \
  "o9RG0BOPhJt+"                                                               \
  "EXHNrC6QCXPMCIEOYERci0McYOkaI8f8OROg4CHSghUEXiAC1VNB7E0rHWXV0GBecjhRMgPYsP" \
  "Z+"                                                                         \
  "tXz8AAORpKHpnRgT4PxUCgdaFLn1bfpig8MvGGe4AAICpXi8pT3291qQDAAAJPjeckw9mQbUYE" \
  "jKGueGYYpATxLywPV+"                                                         \
  "qRESUEBkZ01Rd7ZVLrBNmEzsAAABAMpGQzAxIFpItToiNvYkVi42TDsWBGuKEwDgMtoZjlolJG" \
  "9/leqTFu/"                                                                  \
  "hYLbhrsDUFi+"                                                               \
  "2gE4PqNFSm66oco14twjpVYgIJx4MrJ4Jc64CLFRzHXMnA6whruS7CDNerpnTwllKYBMhkOkAf" \
  "HdGKQDgOXoRg3UuvCqomZ82ka2hluODUte/"                                        \
  "mQC3fQSZMRKaKIioGQ2O1qNbYwNDtG3YREyAASUQxmggzACoi6IUk74JHXC4n0XlE1ofNBcawX" \
  "B/jNOd76rjxMMSt30R2vaduv/"                                                  \
  "TDcquhxZXaMUwyR3gRYoUAR0fpACapOgZjAMCCkyAl47SOQlQX9UbPrclh/"                \
  "sCJYACWn7Vu7Pf71cnkAQDERfpnLAC3RVrsh0GH98kRxlNG/"                           \
  "F1AN2RXSareh0aDAAwvaVKAkwA+B2zFB9EghsWC5zlgKz7QQFjA87EHkLpzR+"              \
  "7Estuuq7vCLnSOscwcAAAAQNFCSYHAJaDFaUlKICYQgxBi4mJiAC1K0SIQZZoWJQIxlEOMiNC1" \
  "3a7A8I+NjcWwtZRD0/HpuI5MjvCaYxg4dBhVTIVN77SlMy69/"                          \
  "gbXoF5YyVxpkRz5hYHAXCThINdxMDwSLua4huR1JJCoMxiknUEnYOUAhrXVOLi45jWTj7HFNWG" \
  "OPGocAzOxMrN0DWQCQxi4eB3HxQHizLNywJKAOo0nkdKAcek9a+"                        \
  "ttYcQ2wQI3wwAAxngnwSoMHDMzxytkDIWprhvO99dwNHc6vccsXRczx8wcr3VjHLH1O0rtnxB6" \
  "3sscGhAXHdCDtHtPCXQMhFAn0QGI7FSPk6M2dPvdgVupidB5eFdkq9M3Af3YoJt6QJKOEHCyp4" \
  "ABq0sII8C1Q+"                                                               \
  "oi6RwPj0SuwCfI9SIAHjcsqQazIHoY4SluWFILdgJe4OmYEAgOMxwGCEBXJ00nhuOkkokbJAAA" \
  "gLioCE3R4hRhSgChi4iIikiKijBFCcULJhCKCsWZiHCFuyjEiZAZbpQog1BgIiqkaQrPKryGa8" \
  "gVjrwIk4thgCRzzU1UbWxsmocJkPBccekpoJQfUUgIrCJK1yk9gMl1RXL6MHoD4hkVjHNFYYWB" \
  "ImpdNILVUdANYbGI+S4P2CBj5PV4MNdk5mBkIDBoTLOmTfR/"                           \
  "pze6Rpu4rWPwyMAMMFGnii7jVkOCYBUlMpGNNxPDCEx3UUfOMMzxmg6xYM4viYQWQykoCaVMMW" \
  "uMD+l9GL8jtYx0wEVIS4uhGUJP1gZCCIOOoEVYDRgyr5mZURKJDkIGPkPr/"                \
  "shgABdMKPorPG8xRIFWCmF7ROs1NMAwwbswp35SMgl5wpgBw3y5frcAAO5RAZ42TC7GyEOjj5e" \
  "lNLKHtGF0MYgJurhaF03q8a7OShARqchMZ86cOU81OVd0V6mYjB6K5RGQAAAAgCQ0MEsI6sIFK" \
  "XDJkiIiQtGKifLpycL/"                                                        \
  "qdOjOVkOjLR3Nm3sdq2qatnFV4pTUdT00l86gWIRFNTE9QjDjIIwSbByTQ4Yrhz13GmBQxfhFA" \
  "RxAkVQVBW5rr8Q5mJWt0LlxBSWFpSZXMegNPNiDoChel1lNACvYZjjmgDHXA+"              \
  "g0ooMHEamoYDxMsdMFsKnUEPH3DbR6PXRFLpI5gAqkWRQwcBwyjh4FJjjQ0NGWCyRO43sAacOQ" \
  "7ikYB1UbzLJKxNGI1zXdjAwsE9nZ1MAAYA1AQAAAAAAg0MAAAgAAABdaUsxHF7/aP9I/1T/"    \
  "Zf9o/2VCaGv/Yf9P/2xFSERse/"                                                 \
  "9GhzM8e5fRoM+b5OAGfAv2HQqqOog9JsfCj3TjREdf7JhGeAIAhCGAs/"                   \
  "3wAwAMjwBAC7pdHQBPIwHifACATdHdiX4sBayk0G+"                                  \
  "19VdyEWOOLXK8AdCFWC0f5wAAFOAfPgesvcRQqFgV4CluWGoKTKAt4Olup2ZJIMvISBlWHnO50" \
  "fWUcDoWmck5igUAAHBjumQpndi5qSAu+"                                           \
  "2RKnKK5XMzEtEIg06kf66Tfx4ZMz1Q3nfoNprOHC5h+kTtLFaUxhFFYm89qYmLZhKF+"        \
  "h0YQywuvFabB5JphZqURU8W0hjYqiF4fhyuXnXqXdRZMigCBCwaGzNaIAWzlgAsmxxwhU1H1uO" \
  "PC4qUaPWDxURWwBwS/"                                                         \
  "g0sFv5AE6KC6KF8Anowg3yMtWjA6qg8HygEwOinpdrtjYsJgSEGGwEiqi3IwZ4lsKElRsRDvAs" \
  "YKwBXmtBk2tbC0ZY1ZXUuPMGGCqMuHp1XHDX3UoC+"                                  \
  "UX9zoGRB19DemuhXgYyIYihW3GykLE9z3UsNEbUEMd2aKFXC69HrP5n+"                   \
  "s3moUYlwUXA9esFLvXI+"                                                       \
  "v7UVde0fwAADUYjKA2KsJGswMFQ6tBX4CAIDMl1N4z1MAAAAqGQCeNvSuBCZgBU9pQ29KYAJW8" \
  "HTXUsoUGSgzDWbMXKnQADGacdErikwJSAAAACmYiEERUkZJEKmgIFrG8BYsrgIqQQkpiNIQgoW" \
  "EOCLKMNX0b2NvYzdV1M6Yzr8gIogDm9ZCTfOJmZCo5RaToSEpnI50Do0jQ4mLuCwweuqE3iPz5" \
  "Ua1UONIruPhDfWhHowMniJMJIs+rNc+UjRohCNPrtYiRLGxmphYta2KPYNxwqnz3BAfSR8+"    \
  "r5vo6BH7ITRKPJgMw55pYxAjtzXdYUtYOUlsOwEPjisZOGZGwknTpdBIFnEBgxW45kCnII+"    \
  "JFgUAnjHMZcoLGIfUpWMggzitIVayXObeOyO49pfWMeKET0sAIgghvjs5ZCDUqW+gI3QB/"     \
  "jrBrcgBIbaerbWY0DEmnJkAACsA6Q2wJcNud1FAB+MQ2PmwhiVkBQD+NuwxBjlBVy+"         \
  "j7elt2GIKLKj1Mm3PhwmwYs6ws1cd1+"                                            \
  "biUjKOMTcBAAAAREXFKFEWEQhFaTFaBJSYQCgUF6NoAYQiFSIQMQ2xsdpYbKyG6ZitgTqy2hoD" \
  "ajVtNXyZExYfJhjcFq4Lo/"                                                     \
  "YiBlAwCmXUtWSuDFcmjxkh3iCUEHPG47pIAklUBSbXQym55pSOaBgbYtG7XNUnRN7O7SOot4ek" \
  "OJjHycaH45rJ9el6TUKMIRfMNa9j7UEIugkBwgdrlTEJYRh3hgQCD4C8DjJkVrjgyFqiixylBs" \
  "4ImGVZRFhpqC6Kl5cybwTJ9CQi4zJOajqANYToGaqJAyAMYQv4ZU64M3TrKELHDYHpidhtgTtW" \
  "IXgAIGtHONjqd2vVRwIAxlAAoETnLWCKxCC6BXQ6AiD0RyBIkQkCFgCEsoTQB6ByY2wrkAmgTS" \
  "bzumo8bcity2wT4AC+"                                                         \
  "NmwiBbtMOg9jeF4DFuGDaSY1Xmw7gN4VKgAiIImU2zEBU1XprrhOwmYsdiAAAAAwRvhcDpfyKB" \
  "VwuVyGAEIxoZtvQjdJAcTFBOI+"                                                 \
  "EXeVSm5lLkogpKUeVnjkuq4Xx3XMmfD6cByzWonS8RjBIqCjq7DAgCNhMoEc36xgMsRFIsLKKP" \
  "ABCwEADqUu0oP3cKhH+AxA7IjucHffIRFg4FKYTBgeUo5LzMp8un6P4+DDIzMcV+"           \
  "b6njKB2nfHGmABk0G6XlgwAHYKHxFdz/"                                           \
  "fSgoOFxpQsgHJPDqlT364nHBXXEhvHAVOlBMObCAfXJsNEW8VNDAHgCtVHTXsYoyNh9EQPco1I" \
  "16PfOos28UYwlLBMdP7qnBTAba2w25wMGRTd9jxoW/"                                 \
  "0LD8AQqtcXKXDqIYsjEgAgwm+MIGhxJJ1HG0DH6KIA2l5GaCOGjt+"                      \
  "N4E7qRgZAqCsiwrsQmRFAyDNdM8DlOOZeAF4HXLwLphj0eSFne64DTjFFZSLq46XIYvB0V0RVC" \
  "aQoInfFkLNLOVeVuE4sZjIHEgAAgJmVkBAkHRwAgZhAjJZKQGZHjBrIgPHZFvNMi8W08cHq7to" \
  "oZ1/"                                                                       \
  "bnTplsEyvk2IyOF4CJZFLDV+"                                                   \
  "EEnDwpfTQqs3eexZGggtd4arG4ZXxXqLMWaquTbRJGLJIb4YBdM5L6XJGxOkrcoVDLqaKnKmDH" \
  "FHHLfYMDEjmhCAXr1TKkXkC1Bi0F5CwRsd0gHoU2T6dggaYyJ0ALvh+"                    \
  "3QpjABFwHKuw3wDovfPSTREWXBdhIe6UxALG+62jzGeQ04qVOSgC5EyahRCl+"              \
  "cENZIZrYG6lHFm03BczCsVIO1pS9MNEh81Dp2s3pv/"                                 \
  "aE7kJo25ZBeT4pml0gTAuF5abHe19EskfAfg+TDuffRgA0NuiuNGEEfjlebmjrE5hN/"        \
  "cBZ0I8SQ9wOket640+"                                                         \
  "FiaiT859USQNpVU8JMgANjZ0ySTjIuACT2NDF10wIxAe4OmuqqEylFkZFVlGWZWZu4c4t1gptZ" \
  "WpvK6EUyUWkdlBJAAAkMYshYwaHkcRRTlLGAIx2lOCEiyniUAgaW6iss8SZevVSrr1uvIalYvh" \
  "V+H6XY/"                                                                    \
  "5I0tvxe+4vskAmWsh9R3utFYXWR+yB+"                                            \
  "K9uTrjhI3Oul6dRKRkRhSxyOgsOYim6ruDEAioHRctK3dGQkYZjRPLZiPqbsZ316ha6WtSm0mL" \
  "82GyBnN3TMBMVDHJwJEhOWK1MnARAw4K89uYGQDEI5TonZ5SXRgAOgpG4TLE0kk9YJHApSNw9j" \
  "A9nTBSHL4Sa/TBpHTpQupIdUSoM3JIT6NvLJm0AteV/"                                \
  "JgrQ2wAHWGSQLgVkDAMlPbqcuQyI5oG1LsXYDxvADWwilwj7ST6RgPMyhMDFAJcSDO8Y4LuqbU" \
  "6XS7ekbjouHmmAwCEugMAAIy/"                                                  \
  "DQA89rVFBlDIcLKVgAVc+UQCDBfOqdq7+"                                          \
  "gkMSOAcdf9k1ZIZEcwrIRkwtMikFa8WOYBv3mUY9FGF9KsIEDiMDnUkmd2DmqD1wTAcH/"      \
  "LhFQCs+STkJIWbf+"                                                           \
  "i1noRUk8LNa4RaNStrliIy7ZmnJZ6KioqqiuB2ZkveACC5lSARpEUIHQGhRMBlROiLPgrqdPjo" \
  "gN2JQIRiBgD4pYZpa2+KQ78cs9rMZCZTzafreLRabNW0cYBiiMDvAXwBBb2KvZavEyjk+"      \
  "wIKepX0LF8nqJBv1ayhrChFxNjsdKRpGaG6jIzMVEZlDWUEIA3SdddcSYkTWjQFFAsYTACQKMZ" \
  "O1oqYJ2Tbmf3GdFJMw960FYemjWlBDQeOY1htBFARwV4cDQTZkZIIGjYUGgX/A/"            \
  "21UBF4jsKEUhJBf2C5eioYLxB6aqyuijIjK7I6iojILMuK6pBFBuw74jgO7aoq9BbPbaS4GiAw" \
  "EgAAkGBJxEzEIEkEFoIhIKWQDAh3SuiC0KHdxMTFhAIxOLJV1wQMW8PE1vTl9eEZxxxhrmMbrx" \
  "/MS0UeK9xSwuXSMUXQbhgycx0Mp3Tw4ZUwD2ASItldCUp1C2ktjhwrXjxkUcoMBh08CsMStQIZ" \
  "nQDIizFhKHXoMJJDBhc8JcQT4vVOC2F1PQgHGRYmpBsjY2hA5nER5rjCVEuuFwGYXKPfDyEIWs" \
  "TohzgCHVjWMyIk478QRFHTYme1mKYgFluL1dZG1c6hYczUsDHf5dN1VmsPXhcHr2EeA5ODQBYp" \
  "wngdgH4bQXdo3T66QMAEwshJAACEwyJ1Obi+"                                       \
  "zPXg03G8clwLdelg6Z2hrgBcj5UOXvn06XHleAsAAIAMAEkAdyjABX42lE6HPoLaZ1POlTwbGi" \
  "dCjaD2kZwr+"                                                                \
  "Q4VkQA4ogN7WiemLlehh56ejBAssocIkyQAAICQb4yDR8GCoeAR0CK0UITQRETANGFRyG7sC9x" \
  "lSkTSjRIQERG1ADaIvxETVfXU8GMKID6iYpp4BMIlKmBQuH4TQBuSNXhFbSCTSTLkwyMQwszMg" \
  "o54Z0R0hpQ0AZ0OnuIiw29a8ABmGNJkDg4e4YTeqbNokeh0xmnC5y0gpJ5Bj1BvPs11ZEmzJMb" \
  "z0G0v3YFhwKJTTyIc6kOJq+d5GOFVDZiECVpHgA7oATQoo97QCBCXHhkmB0wy5NMxJK8Pr+"    \
  "QKcxw1bQzDtLG3KFZMBXkNC5lkEljwgBMUGFoAAMYagAkmIqDbIQCEgF8Ljhzp0RE2AwAAbSa/" \
  "ZIXJfHMB83oRAAAAafi2AExM4JxtN/"                                             \
  "DESd8CtKpCoqoscAA2BjReBHlA7Dr+"                                             \
  "HNEY0HgZmCD2ncGXE7prqIrKSFkZkRk1tW2cs2HvdjbD0cRyrq2T6rCxiakgAQAABKQRJAlmkk" \
  "JAKBAVF5cKZFFxSkwoTmgxIRGAZjEKtCglJipmY2uLrWKI/"                            \
  "WiYtn7Zy+"                                                                  \
  "tWveCVuSbHdTBz5XjNdT2GZpl8RujoCNPweHEwMGGYzMzxLEshzDGZLImB15OIsBEYhvB5gsDM" \
  "EeAHiGJoD07S7dBLmNetmMcpkSOvJGRmuOA1gdKPvI4jMzw+"                           \
  "HT2BsUB1ZqLRRh6A4eARZq6FAN8SsJ4ajPLrBOBC54kTicW7LXMgsZANqjM90+ucyIQE+"      \
  "H1DFQcMemZmH4mh9q0wc7wmU+kDx5UXMQVggMzpNHovTChAe/"                          \
  "EA6feMuge9d3rJlADvHSxK6Mpo0EYgjT+"                                          \
  "lN4DLlZ4t1YUZJwBdgogOevvBVmv29EsXHcYEAIb4qOJcu5TvtVOAaYiBALGE0ykWcDNs+"     \
  "VALJFMcp9F1Fk+"                                                             \
  "wwJmi2BpdTNSsUUrBItgBxJ6CAOCx8ID52yQZjpnXdV0cwlCG2DCmqaY9hnRcJ6cWSr8wmTurP" \
  "gl89cRIzKBFcO739eUTHFGDJuXU77q8qhII6hA6APgEAp6Qo7xFV495hVEj1zVWI5KLtbUbJY/" \
  "jlQdd0/"                                                                    \
  "cyGKjgGIbvhVNcCwBs+"                                                        \
  "VDDUEANE7nG1o6SDAnUMLFTeVERQTLhCAcQMpQPwmGZLNoaoa4IXSj0kbpjcmOkR7ggzG8dydA" \
  "bPgixyHjNx8yaACT9fEbDNTnZ7ZPPa0FWTGfW00NVR9TbZtbUtiluzGZGAQTQv7AkEoSmiIebh" \
  "zhAIABlEHq60XRvRYQoY0q0EDOtDqfMqPamA79tpsPWCccc88ehQ6ti2Nk4cGTnt8UUtbWO9j5" \
  "WHEdmHh+7C9wIRcbudjS9CXjM7kaocWm3CfsmIKfZpvtHUVaURZ6uy8pxxLmzR5SEHNsXspeo+" \
  "DDzw4x9MaFSHm6tIyWiFempO9P2dtOMiWnTObTzx19O+Msvi/htb6NqsZ/"                 \
  "O3s7eMiJWj9dTFq5AEM9w6Lh/X076mm6aA9sCSIoJAHo13PxwXzBABvcU8c/"               \
  "u7iQHUUTgzNM0Wo1dlmJRzDKZSAAAAAAwJwLO5a/4yl+5v++H209u/"                     \
  "94v13Pn59nDcvtSL21xLUpndfvy7VA5//"                                          \
  "w4f27LOL795PYo+f7974+"                                                      \
  "SXupFFhk591zTOL79eH98nts4eItR07Oe9axnPeuZE0CZ27tPW1u3OzOKit91rOe+"          \
  "1K7jFmVbZRyPx0AxaDGNyYnJp/7+eXL7yaB/9H/Pku1wvejpgenJsPvz+/"                 \
  "+mrSgjkevxerzVlbPImoxrYbKeY8BJsnrRY89KMmG12IoUvs3b7eanpycFiVHdouYWg8lsNmt1" \
  "FS+33+9zIiFVOq5jbU9nZ1MABYA2AQAAAAAAg0MAAAkAAACfd/skAWoWAKqz3++H+/"         \
  "T0pGA6hrVD9XUrrc4zc3Nzc/r9sOZyIGq6bDnpYwKensDQ9y/"                          \
  "K37+cAFtbW3TudhWOtwZzlstb/X5/a6vf72/"                                       \
  "x5+fm5nB6slBlZ3Fcha363d5ut7u3ni1rLoPf728l3KcK"

namespace {

const int kToggleCount = 4;
const int kNumChannels = 2;
const int kSampleRate = 44100;
const int kFramesPerBuffer = 882;  // 10ms
const CefAudioHandler::ChannelLayout kChannelLayout = CEF_CHANNEL_LAYOUT_STEREO;

const char kAudioOutputTestUrl[] = "http://tests/audiooutputtest";
const char kAudioCloseBrowserTestUrl[] = "http://tests/audioclosebrowsertest";
const char kAudioTogglePlaybackTestUrl[] =
    "http://tests/audiotoggleplaybacktest";

const char kTestHtml[] =
    "<!DOCTYPE html><html><head><meta "
    "charset=\"utf-8\"/></head><body><p>TEST</p><audio "
    "id=\"audio_output_frame\" src=\"" AUDIO_DATA
    "\" autoplay "
    "style=\"display:none\"></audio></body></html>";

const char kToggleTestHtml[] =
    "<!DOCTYPE html><html><head><meta charset=\"utf-8\"/>"
    "<script type=\"text/javascript\">"
    "var timeouts = [150, 1950, 150, 2000, 150, 2050, 150, 2100, 150, 2200, "
    "150];"
    "var count = 0;"
    "function togglePlayback(el, playing, count) {"
    "  if (playing) {"
    // "    console.log('togglePlayback pause (' + count + ')');"
    "    el.pause();"
    "  } else {"
    // "    console.log('togglePlayback play (' + count + ')');"
    "    el.play();"
    "  }"
    "}"
    "function loadHandler() {"
    "  var el = document.getElementById(\"audio_output_frame\");"
    "  el.onplay = (event) => {"
    "    var timeout = timeouts[count];"
    // "    console.log('loadHandler onplay (' + count + ') wait ' + timeout);"
    "    if (count < timeouts.length) {"
    "      setTimeout(togglePlayback, timeout, el, true, count);"
    "      count++;"
    "    }"
    "  };"
    "  el.onpause = (event) => {"
    "    var timeout = timeouts[count];"
    // "    console.log('loadHandler onpause (' + count + ') wait ' + timeout);"
    "    if (count < timeouts.length) {"
    "      setTimeout(togglePlayback, timeout, el, false, count);"
    "      count++;"
    "    }"
    "  };"
    "}"
    "</script></head>"
    "<body onload=\"loadHandler()\"><p>TEST</p>"
    "<audio id=\"audio_output_frame\" src=\"" AUDIO_DATA
    "\" autoplay style=\"display:none\"></audio></body></html>";

class AudioOutputTest : public ClientAppBrowser::Delegate {
 public:
  AudioOutputTest() {}

  void OnBeforeCommandLineProcessing(
      CefRefPtr<ClientAppBrowser> app,
      CefRefPtr<CefCommandLine> command_line) override {
    // Allow media to autoplay without requiring user interaction.
    command_line->AppendSwitchWithValue("autoplay-policy",
                                        "no-user-gesture-required");
  }

 protected:
  IMPLEMENT_REFCOUNTING(AudioOutputTest);
};

class AudioTestHandler : public TestHandler, public CefAudioHandler {
 public:
  AudioTestHandler() {}

  void SetupAudioTest(const std::string& testUrl) {
    // Add the resource.
    AddResource(testUrl, kTestHtml, "text/html");

    // Create the browser.
    CreateBrowser(testUrl);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    browser_ = browser;
    TestHandler::OnAfterCreated(browser);
  }

  CefRefPtr<CefAudioHandler> GetAudioHandler() override { return this; }

  bool GetAudioParameters(CefRefPtr<CefBrowser> browser,
                          CefAudioParameters& params) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_UI));
    params.channel_layout = kChannelLayout;
    params.sample_rate = kSampleRate;
    params.frames_per_buffer = kFramesPerBuffer;
    got_audio_parameters_.yes();
    return true;
  }

  void OnAudioStreamStarted(CefRefPtr<CefBrowser> browser,
                            const CefAudioParameters& params,
                            int channels) override {
    EXPECT_FALSE(got_on_audio_stream_started_);
    EXPECT_TRUE(got_audio_parameters_);
    EXPECT_TRUE(browser_->IsSame(browser));
    EXPECT_EQ(channels, kNumChannels);
    EXPECT_EQ(params.channel_layout, kChannelLayout);
    EXPECT_EQ(params.sample_rate, kSampleRate);
    EXPECT_EQ(params.frames_per_buffer, kFramesPerBuffer);
    got_on_audio_stream_started_.yes();
  }

  void OnAudioStreamPacket(CefRefPtr<CefBrowser> browser,
                           const float** data,
                           int frames,
                           int64 pts) override {
    EXPECT_TRUE(got_on_audio_stream_started_);
    EXPECT_TRUE(browser_->IsSame(browser));
    EXPECT_EQ(frames, kFramesPerBuffer);
    got_on_audio_stream_packet_.yes();
  }

  void OnAudioStreamStopped(CefRefPtr<CefBrowser> browser) override {
    EXPECT_FALSE(got_on_audio_stream_stopped_);
    EXPECT_TRUE(got_on_audio_stream_started_);
    EXPECT_TRUE(browser_->IsSame(browser));
    EXPECT_TRUE(CefCurrentlyOn(TID_UI));
    got_on_audio_stream_stopped_.yes();
    DestroyTest();
  }

  void OnAudioStreamError(CefRefPtr<CefBrowser> browser,
                          const CefString& message) override {
    LOG(WARNING) << "OnAudioStreamError: message = " << message << ".";
    got_on_audio_stream_error_.yes();
    DestroyTest();
  }

 protected:
  void DestroyTest() override {
    browser_ = nullptr;
    EXPECT_TRUE(got_audio_parameters_);
    EXPECT_TRUE(got_on_audio_stream_started_);
    EXPECT_TRUE(got_on_audio_stream_packet_);
    EXPECT_TRUE(got_on_audio_stream_stopped_);
    EXPECT_FALSE(got_on_audio_stream_error_);
    TestHandler::DestroyTest();
  }

  CefRefPtr<CefBrowser> browser_;
  TrackCallback got_on_audio_stream_started_;
  TrackCallback got_on_audio_stream_packet_;
  TrackCallback got_on_audio_stream_stopped_;
  TrackCallback got_on_audio_stream_error_;
  TrackCallback got_audio_parameters_;
};

// A common base class for audio output tests.
class AudioOutputTestHandler : public AudioTestHandler {
 public:
  AudioOutputTestHandler() {}

  void RunTest() override {
    // Setup the resource.
    SetupAudioTest(kAudioOutputTestUrl);
  }

  void OnAudioStreamPacket(CefRefPtr<CefBrowser> browser,
                           const float** data,
                           int frames,
                           int64 pts) override {
    if (!got_on_audio_stream_packet_.isSet()) {
      browser->GetMainFrame()->ExecuteJavaScript(
          "var ifr = document.getElementById(\"audio_output_frame\"); "
          "ifr.parentNode.removeChild(ifr);",
          CefString(), 0);
    }
    AudioTestHandler::OnAudioStreamPacket(browser, data, frames, pts);
  }

 protected:
  IMPLEMENT_REFCOUNTING(AudioOutputTestHandler);
};

class AudioCloseBrowserTest : public AudioTestHandler {
 public:
  AudioCloseBrowserTest() {}

  void RunTest() override {
    // Setup the resource.
    SetupAudioTest(kAudioCloseBrowserTestUrl);
  }

  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
    EXPECT_FALSE(got_on_audio_stream_stopped_);
    TestHandler::OnBeforeClose(browser);
  }

  void OnAudioStreamPacket(CefRefPtr<CefBrowser> browser,
                           const float** data,
                           int frames,
                           int64 pts) override {
    if (!got_on_audio_stream_packet_.isSet()) {
      CloseBrowser(browser, true);
    }
    AudioTestHandler::OnAudioStreamPacket(browser, data, frames, pts);
  }

 protected:
  IMPLEMENT_REFCOUNTING(AudioCloseBrowserTest);
};

// kToggleCount represents the times the OnAudioStreamStarted and
// OnAudioStreamStopped should have been called.
// Both get called on playback start and end respectively. As well as when there
// is a period of silence for more than 2 seconds (see
// CefBrowserHostImpl::OnAudioStateChangedand and
// CefBrowserHostImpl::OnRecentlyAudibleTimerFired). The timeouts in kTestHtml
// represent play and pause timeouts. So for example it should play for 150ms
// and then pause for 1950ms. In this example OnAudioStreamStopped must not be
// called because it is below the 2 seconds threshold. So in this 10 timeouts
// there are exactly 3 which should trigger OnAudioStreamStarted and
// OnAudioStreamStopped together with the initial stream start and the final
// stream stop. And due to the need to test the toggle bounderies it results in
// this nearly 15 seconds test run.
class AudioTogglePlaybackTest : public AudioTestHandler {
 public:
  AudioTogglePlaybackTest() {}

  void RunTest() override {
    // Add the resource.
    AddResource(kAudioTogglePlaybackTestUrl, kToggleTestHtml, "text/html");

    // Create the browser.
    CreateBrowser(kAudioTogglePlaybackTestUrl);

    // Time out the test after a reasonable period of time.
    SetTestTimeout(20000);
  }

  void OnAudioStreamStarted(CefRefPtr<CefBrowser> browser,
                            const CefAudioParameters& params,
                            int channels) override {
    EXPECT_TRUE(browser_->IsSame(browser));
    EXPECT_EQ(channels, kNumChannels);
    EXPECT_EQ(params.channel_layout, kChannelLayout);
    EXPECT_EQ(params.sample_rate, kSampleRate);
    EXPECT_EQ(params.frames_per_buffer, kFramesPerBuffer);
    got_on_audio_stream_started_.yes();
    start_count_++;
  }

  void OnAudioStreamStopped(CefRefPtr<CefBrowser> browser) override {
    EXPECT_EQ(start_count_, ++stop_count_);
    if (stop_count_ == kToggleCount)
      AudioTestHandler::OnAudioStreamStopped(browser);
  }

 protected:
  int start_count_ = 0;
  int stop_count_ = 0;
  IMPLEMENT_REFCOUNTING(AudioTogglePlaybackTest);
};

}  // namespace

// Test audio output callbacks called on valid threads.
TEST(AudioOutputTest, AudioOutputTest) {
  CefRefPtr<AudioOutputTestHandler> handler = new AudioOutputTestHandler();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test audio stream stopped callback is called on browser close.
TEST(AudioOutputTest, AudioCloseBrowserTest) {
  CefRefPtr<AudioCloseBrowserTest> handler = new AudioCloseBrowserTest();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test audio stream starts/stops properly on certain time bounderies.
TEST(AudioOutputTest, AudioTogglePlaybackTest) {
  CefRefPtr<AudioTogglePlaybackTest> handler = new AudioTogglePlaybackTest();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Entry point for creating audio output test objects.
// Called from client_app_delegates.cc.
void CreateAudioOutputTests(ClientAppBrowser::DelegateSet& delegates) {
  delegates.insert(new AudioOutputTest);
}
