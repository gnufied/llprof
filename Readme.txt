
* llprof version 0.2
 

** ディレクトリ構成
 pyllprof/              Python用プロファイリングモジュール   
   setup.py             セットアップ用スクリプト
 rrprofext/             Ruby用プロファイリングモジュール
   extconf.rb           Makefile生成スクリプト
 monitor/               モニタプログラムのディレクトリ
   RRProf               モニタプログラムのソースコード
   icons                モニタプログラムで使用される画像
 rrprof_monitor.jar     モニタプログラム本体


* プロファイリングモジュールのコンパイル方法
ビルド用のスクリプト(build_***.sh)があるので、それを使用するか参考にして
コンパイルを行ってください。


* プロファイリングモジュールの使用方法
Ruby用もPython用も対応するモジュールを読み込ませることで
実行させることができます。

** Python用モジュール
本プロファイラモジュール(pyllprof)をimportしてください。

** Ruby用モジュール
本プロファイラモジュール(rrprof)をrequireしてください。
一つの方法としては、環境変数RUBYOPTに-rオプションを指定して実行する方法があります。
以下に実行例を示します。
(本プロファイラ＋rdocの実行例)
$ export RUBYOPT="-r rrprof"
$ rdoc

* モニタプログラムの使用方法
モニタプログラムは実行スクリプト (run_monitor.sh) を使用して起動してください。

** プロファイリングモジュールへ接続する
メニューから「Connect」を選択し、プロファイリングモジュールを実行しているホストに対して接続することで
情報の表示を開始することができます。
モニタプログラムのソースコードはmonitorディレクトリ以下のRRProfにあります。

** プロファイリングモジュールからの接続を受け付ける
モニタはポート12300でプロファイリングモジュールからの接続を受け付けています。
環境変数を設定することでプロファイリングもジュール側から接続することができます。
(実行例)
# リストに表示されるプログラム名はprogram1
export LLPROF_PROFILE_TARGET_NAME="program1"
# モニタは192.168.0.10で実行されている
export LLPROF_AGG_HOST="192.168.0.10" 
# 10秒間隔で再接続
export LLPROF_AGG_INTERVAL="10" 
python3 program1.py


* 環境変数一覧

LLPROF_PROFILE_TARGET_NAME      プロファイルターゲット名
LLPROF_AGG_HOST                 アグレッシブモード時の接続先ホスト名
LLPROF_AGG_PORT                 アグレッシブモード時の接続先ポート
LLPROF_AGG_INTERVAL             アグレッシブモード時の接続時間間隔(秒)

