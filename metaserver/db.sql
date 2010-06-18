SET SQL_MODE="NO_AUTO_VALUE_ON_ZERO";

CREATE TABLE IF NOT EXISTS `blacklist` (
  `id_blacklist` int(11) NOT NULL AUTO_INCREMENT,
  `hostname` varchar(80) DEFAULT NULL,
  PRIMARY KEY (`id_blacklist`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 AUTO_INCREMENT=1 ;

CREATE TABLE IF NOT EXISTS `servers` (
  `id_server` int(11) NOT NULL AUTO_INCREMENT,
  `ip_address` varchar(80) DEFAULT NULL,
  `port` int(11) NOT NULL DEFAULT '13327',
  `hostname` varchar(80) DEFAULT NULL,
  `num_players` int(11) DEFAULT NULL,
  `version` varchar(20) DEFAULT NULL,
  `text_comment` varchar(256) DEFAULT NULL,
  `last_update` int(21) NOT NULL,
  `players` text NOT NULL,
  `name` varchar(80) DEFAULT NULL,
  `roworder` mediumint(8) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id_server`)
) ENGINE=MyISAM  DEFAULT CHARSET=utf8 AUTO_INCREMENT=1 ;
