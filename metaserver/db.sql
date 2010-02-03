SET SQL_MODE="NO_AUTO_VALUE_ON_ZERO";

CREATE TABLE IF NOT EXISTS `blacklist` (
  `id_blacklist` int(11) NOT NULL AUTO_INCREMENT,
  `hostname` varchar(80) DEFAULT NULL,
  PRIMARY KEY (`id_blacklist`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 AUTO_INCREMENT=1 ;

CREATE TABLE IF NOT EXISTS `servers` (
  `id_server` int(11) NOT NULL AUTO_INCREMENT,
  `ip_address` varchar(80) DEFAULT NULL,
  `port` int(11) NOT NULL,
  `hostname` varchar(80) DEFAULT NULL,
  `num_players` int(11) DEFAULT NULL,
  `version` varchar(20) DEFAULT NULL,
  `text_comment` varchar(256) DEFAULT NULL,
  `last_update` int(21) DEFAULT NULL,
  `players` text NOT NULL,
  `roworder` mediumint(8) NOT NULL,
  PRIMARY KEY (`id_server`),
  UNIQUE KEY `hostname` (`hostname`)
) ENGINE=MyISAM  DEFAULT CHARSET=utf8 AUTO_INCREMENT=1 ;
