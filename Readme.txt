
* rrprof - Ruby Real-time profiler

** ディレクトリ構成
 rrprofext/              プロファイリングモジュールのディレクトリ
   extconf.rb            Makefile生成スクリプト
   *.cpp;*.h             プロファイリングモジュールのソースコード
 monitor/                モニタプログラムのディレクトリ
   rrprof_monitor.jar    モニタプログラム本体
   RRProf                モニタプログラムのソースコード
   icons                 モニタプログラムで使用される画像


* プロファイリングモジュールのコンパイル方法
通常のRubyモジュールのとおりにコンパイルすることができます。
rrprofextがRubyモジュールのディレクトリで、extconf.rbを実行したのち、
make && make installでインストールすることができます。

(実行例)
$ cd rrprofext
$ ruby extconf.rb
$ make
$ make install

* プロファイリングモジュールの使用方法
本モジュールを読み込ませることで実行させることができます。
一つの方法としては、環境変数RUBYOPTに-rオプションを指定して実行する方法があります。
以下に実行例を示します。
(本プロファイラ＋rdocの実行例)
$ export RUBYOPT="-r rrprof"
$ rdoc

* モニタプログラムの使用方法
モニタプログラムはmonitorディレクトリ以下にあるrrprof_monitor.jarを実行することで
使用することができます。
メニューから「Connect」を選択し、プロファイリングモジュールを実行しているホストに対して接続することで
情報の表示を開始することができます。
ソースコードはmonitorディレクトリ以下のRRProfにあります。



