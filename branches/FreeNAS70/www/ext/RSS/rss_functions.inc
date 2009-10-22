<?php
require_once('util.inc');
require_once('config.inc');
require_once('rss_class_history.php');

define('VERBOSE_OFF', 0);
define('VERBOSE_ERROR', 1);
define('VERBOSE_SUCCESS', 2);
define('VERBOSE_EXTRA', 3);

$TRANSMISSION = array(
    'user' => (isset($config['bittorrent']['username']) ? $config['bittorrent']['username'] : 'admin'),
    'pass' => $config['bittorrent']['password'],
    'downloaddir' => $config['bittorrent']['downloaddir'],
    'port' => $config['bittorrent']['port']
);

function rss_log($message, $err_level) {
    global $config;
    if (isset($config['rss']['debug']) && $config['rss']['debug'] >= $err_level)
    {
        if (isset($config['rss']['debuglog'])) {
            $fp = fopen("{$config['rss']['path']}/rss.log", 'a');
            if (!$fp) return;
            $log = sprintf("[%s] %s\r\n", date('d M Y H:i:s'), $message);
            fwrite($fp, $log);
            fclose($fp);
        }
        else
            write_log("RSS: $message");
    }
}

function rss_download($url, $cookie) {
  $cntx = stream_context_create(array(
    'http' => array(
      'header' => "Cookie: $cookie\r\n" .
        "User-Agent: FreeNAS RSS Extension/1.0 (FreeNAS; FreeBSD)\r\n",
    )
  ));

  $data = file_get_contents($url, false, $cntx);
  $cntx = null;
  return $data;
}

function add_torrent($torrent, $folder, $cookie='') {
    global $config, $TRANSMISSION;
    
    $file = '/tmp/' . md5($torrent) . '.torrent';
    
    $torrent_data = rss_download($torrent, $cookie);
    
    if ($torrent_data === false) {
        $retVal = -1;
    } else if (file_put_contents($file, $torrent_data) === false) {
        $retVal = -3;
    }
    else { 
        $start = "transmission-remote {$TRANSMISSION['port']} --auth={$TRANSMISSION['user']}:{$TRANSMISSION['pass']}";
        $cmd = $start;
        
        if (!empty($folder) && is_dir($folder))
            $cmd .= ' --download-dir=' . escapeshellarg($folder);
        
        $cmd .= " -a $file 2>&1";
        mwexec2($cmd, $output, $retVal);
        
        preg_match('/ responded: "([^"]+)"/i', $output[!empty($folder) && is_dir($folder) ? 1 : 0], $match);
        if ($match[1] != "success") $retVal = "-2 {$match[1]}";

        // just to be safe, set everything back to the default download directory
        $cmd = $start . ' --download-dir=' . escapeshellarg($TRANSMISSION['downloaddir']). ' 2>&1';
        exec($cmd);
        unlink($file);
    }
    
    if ($retVal == 0) rss_log("Downloaded $torrent", VERBOSE_SUCCESS);
    else if ($retVal == -1) rss_log("Unable to download $torrent", VERBOSE_ERROR);
    else if ($retVal == -2) rss_log("Transmission error on $torrent: $ret", VERBOSE_ERROR);
    return $retVal;        
}

function get_by_uuid($array, $uuid, $field = null) {
  $i = 0;
	if (!is_array($array)) return null;
	
  foreach ($array as $item) {
    if ($item['uuid'] == $uuid) {
      if ($field == null) return $item;
      else if ($field == 'id') return $i;
      else if (isset($field) && isset($item[$field])) return $item[$field];
      break;
    }
    $i++;
  }
  return null;
}

function usort_by_pubdate($a, $b) {
  return strtotime($a['pubDate']) < strtotime($b['pubDate']);
}