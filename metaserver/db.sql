CREATE TABLE IF NOT EXISTS `blacklist` (
  `id_blacklist` int(11) NOT NULL auto_increment,
  `hostname` varchar(80) default NULL,
  PRIMARY KEY  (`id_blacklist`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 AUTO_INCREMENT=1 ;

CREATE TABLE IF NOT EXISTS `servers` (
  `id_server` int(11) NOT NULL auto_increment,
  `ip_address` varchar(80) default NULL,
  `port` int(11) NOT NULL,
  `hostname` varchar(80) default NULL,
  `num_players` int(11) default NULL,
  `version` varchar(20) default NULL,
  `text_comment` varchar(256) default NULL,
  `last_update` int(21) default NULL,
  PRIMARY KEY  (`id_server`),
  UNIQUE KEY `hostname` (`hostname`)
) ENGINE=MyISAM  DEFAULT CHARSET=utf8 AUTO_INCREMENT=1 ;
