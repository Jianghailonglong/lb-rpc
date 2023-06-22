USE dns;

SET @time = UNIX_TIMESTAMP(NOW());

DELETE FROM RouteData WHERE modid = 1 and cmdid = 1 and serverip = 3232235953 and serverport = 9999;

UPDATE RouteVersion SET version = @time WHERE id = 1;

INSERT INTO RouteChange(modid, cmdid, version) VALUES(1, 1, @time);